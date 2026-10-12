#include "BodycamWeaponPoseComponent.h"

#include "BodycamCameraComponent.h"
#include "BodycamLog.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	/** 0 below 0, x - b/2 above b, quadratic ease in between (C1 continuous). Used for the pitch lock corner. */
	double SoftPositive(double X, double Blend)
	{
		if (X <= 0.0)
		{
			return 0.0;
		}
		if (Blend <= KINDA_SMALL_NUMBER || X >= Blend)
		{
			return X - 0.5 * Blend;
		}
		return X * X / (2.0 * Blend);
	}

	double SmoothStep01(double A)
	{
		A = FMath::Clamp(A, 0.0, 1.0);
		return A * A * (3.0 - 2.0 * A);
	}
}

UBodycamWeaponPoseComponent::UBodycamWeaponPoseComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UBodycamWeaponPoseComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	Camera = Owner ? Owner->FindComponentByClass<UBodycamCameraComponent>() : nullptr;
	if (!Camera)
	{
		UE_LOG(LogBodycam, Warning, TEXT("WeaponPose on %s: no BodycamCameraComponent found."), *GetNameSafe(Owner));
		return;
	}

	// Read this frame's camera transform, and run after the sway so nothing else writes under us.
	AddTickPrerequisiteComponent(Camera);

	if (!Pivot)
	{
		FindTarget();
	}
}

void UBodycamWeaponPoseComponent::FindTarget()
{
	AActor* Owner = GetOwner();
	if (!Owner || TargetComponentName.IsNone())
	{
		return;
	}

	TArray<USceneComponent*> SceneComponents;
	Owner->GetComponents(SceneComponents);
	for (USceneComponent* Component : SceneComponents)
	{
		if (Component && Component->GetFName() == TargetComponentName)
		{
			Pivot = Component;
			return;
		}
	}
}

void UBodycamWeaponPoseComponent::SetTarget(USceneComponent* NewPivot)
{
	Pivot = NewPivot;
	bWarnedNoPivot = false;
}

void UBodycamWeaponPoseComponent::SetSprintLockSuppressed(bool bNewSuppressed)
{
	bSuppressed = bNewSuppressed;
}

void UBodycamWeaponPoseComponent::UpdateSprint(float DeltaTime)
{
	bool bWantLock = false;

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (bEnableSprintLock && !bSuppressed && Movement && Movement->IsMovingOnGround())
	{
		const float Threshold = bSprintLatched ? SprintSpeedThreshold * SprintReleaseRatio : SprintSpeedThreshold;
		bWantLock = Movement->Velocity.Size2D() >= Threshold;
	}
	bSprintLatched = bWantLock;

	const float Time = bWantLock ? LockBlendInTime : LockBlendOutTime;
	const float Step = DeltaTime / FMath::Max(Time, 0.01f);
	SprintAlpha = bWantLock ? FMath::Min(SprintAlpha + Step, 1.f) : FMath::Max(SprintAlpha - Step, 0.f);
}

FTransform UBodycamWeaponPoseComponent::ComputeBaseFrame() const
{
	const FTransform CameraFrame = Camera->GetComponentTransform();
	const double Alpha = SmoothStep01(SprintAlpha);
	if (Alpha <= 0.0)
	{
		return CameraFrame;
	}

	// Stable frame: aim pitch (clamped at LockPitch), view yaw, no roll, no bob. The camera position without bob.
	const double AimPitch = FRotator::NormalizeAxis(Camera->GetAimRotation().Pitch);
	const double Excess = SoftPositive(AimPitch - LockPitch, LockSoftness) * Alpha;
	const FRotator StableRotator(AimPitch - Excess, CameraFrame.Rotator().Yaw, 0.0);
	const FQuat StableQuat = StableRotator.Quaternion();
	const FTransform StableFrame(StableQuat, Camera->GetAimLocation());

	FTransform Frame;
	Frame.SetLocation(FMath::Lerp(CameraFrame.GetLocation(), StableFrame.GetLocation(), Alpha));
	Frame.SetRotation(FQuat::Slerp(CameraFrame.GetRotation(), StableFrame.GetRotation(), Alpha).GetNormalized());
	Frame.SetScale3D(FVector::OneVector);

	// Low-ready pose inside that frame (pose first, then frame).
	const FTransform Pose(
		FQuat::Slerp(FQuat::Identity, SprintRotation.Quaternion(), Alpha),
		SprintOffset * Alpha);
	return Pose * Frame;
}

void UBodycamWeaponPoseComponent::RunProbes(const FVector& Eye, const FTransform& BaseFrame, float& OutPenetration, float& OutTilt, float& OutYaw) const
{
	OutPenetration = 0.f;
	OutTilt = 0.f;
	OutYaw = 0.f;

	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BodycamWeaponProbe), false, Owner);
	TArray<AActor*> Attached;
	Owner->GetAttachedActors(Attached);
	Params.AddIgnoredActors(Attached);

	const FVector Muzzle = BaseFrame.TransformPosition(FVector(WeaponReach, 0.0, 0.0));
	const FVector Right = BaseFrame.GetUnitAxis(EAxis::Y);
	const FVector Up = BaseFrame.GetUnitAxis(EAxis::Z);
	const FCollisionShape Shape = FCollisionShape::MakeSphere(ProbeRadius);

	enum EProbe { Centre, High, Low, Left, Right_, Count };
	const FVector Ends[Count] = {
		Muzzle,
		Muzzle + Up * ProbeSpread,
		Muzzle - Up * ProbeSpread,
		Muzzle - Right * ProbeSpread,
		Muzzle + Right * ProbeSpread,
	};

	float Penetration[Count] = {};
	for (int32 i = 0; i < Count; ++i)
	{
		const double Length = (Ends[i] - Eye).Size();
		FHitResult Hit;
		const bool bHit = World->SweepSingleByChannel(Hit, Eye, Ends[i], FQuat::Identity, TraceChannel, Shape, Params);
		Penetration[i] = bHit ? FMath::Clamp<float>(Length - (Hit.bStartPenetrating ? 0.0 : Hit.Distance), 0.f, WeaponReach) : 0.f;

		if (bDebugDraw)
		{
			DrawDebugLine(World, Eye, Ends[i], bHit ? FColor::Red : FColor::Green, false, 0.f, 0, 0.4f);
			DrawDebugSphere(World, Ends[i], ProbeRadius, 8, bHit ? FColor::Red : FColor::Green, false, 0.f);
		}
	}

	OutPenetration = FMath::Max(FMath::Max3(Penetration[Centre], Penetration[High], Penetration[Low]), FMath::Max(Penetration[Left], Penetration[Right_]));
	OutTilt = Penetration[Centre] * TiltPerCm + (Penetration[Low] - Penetration[High]) * TiltPerCm;
	OutYaw = (Penetration[Left] - Penetration[Right_]) * YawPerCm;
}

void UBodycamWeaponPoseComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Camera || DeltaTime <= 0.f)
	{
		return;
	}
	if (!Pivot)
	{
		FindTarget();
		if (!Pivot)
		{
			if (!bWarnedNoPivot)
			{
				UE_LOG(LogBodycam, Warning, TEXT("WeaponPose on %s: pivot component '%s' not found (see README section 8)."), *GetNameSafe(GetOwner()), *TargetComponentName.ToString());
				bWarnedNoPivot = true;
			}
			return;
		}
	}

	const double Dt = FMath::Min(DeltaTime, 0.1f);

	UpdateSprint(Dt);
	const FTransform BaseFrame = ComputeBaseFrame();

	// Collision against the pose the weapon would have without it.
	float RawPenetration = 0.f;
	float Tilt = 0.f;
	float Yaw = 0.f;
	if (bEnableCollision)
	{
		RunProbes(Camera->GetAimLocation(), BaseFrame, RawPenetration, Tilt, Yaw);
	}

	const bool bGoingIn = RawPenetration >= SmoothedPenetration;
	SmoothedPenetration = bGoingIn ? RawPenetration : FMath::FInterpTo(SmoothedPenetration, RawPenetration, Dt, 8.f);

	const FVector PushTarget(
		FMath::Min<double>(RawPenetration * PushBackRatio, MaxPushBack),
		FMath::Clamp<double>(Tilt, -MaxTilt, MaxTilt),
		FMath::Clamp<double>(Yaw, -MaxYaw, MaxYaw));
	PushSpring.Step(PushTarget, Dt, bGoingIn ? PushFrequency : ReleaseFrequency, CollisionDamping);

	// Collision offset in the base frame: rotate (tilt up / yaw right) around the eye, then pull back along the weapon axis.
	const FTransform Collision(
		FRotator(PushSpring.Position.Y, PushSpring.Position.Z, 0.0).Quaternion(),
		FVector(-PushSpring.Position.X, 0.0, 0.0));
	const FTransform Final = Collision * BaseFrame;

	const USceneComponent* Parent = Pivot->GetAttachParent();
	Pivot->SetRelativeTransform(Parent ? Final.GetRelativeTransform(Parent->GetComponentTransform()) : Final);

	if (bDebugDraw && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			static_cast<uint64>(reinterpret_cast<UPTRINT>(this)) & 0x7FFFFFFF, 0.f, FColor::Cyan,
			FString::Printf(TEXT("WeaponPose: sprint %.2f%s  penetration %.1f cm%s  push %.1f tilt %.1f yaw %.1f"),
				SprintAlpha, bSprintLatched ? TEXT(" LOCK") : TEXT(""), SmoothedPenetration,
				IsWeaponBlocked() ? TEXT(" BLOCKED") : TEXT(""),
				PushSpring.Position.X, PushSpring.Position.Y, PushSpring.Position.Z));
	}
}
