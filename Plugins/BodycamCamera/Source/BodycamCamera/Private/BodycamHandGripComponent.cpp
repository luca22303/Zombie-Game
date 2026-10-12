#include "BodycamHandGripComponent.h"

#include "BodycamLog.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "GameFramework/Actor.h"

namespace
{
	struct FIKResult
	{
		FTransform Upper;
		FTransform Lower;
		FTransform Hand;
	};

	/**
	 * Analytic two-bone IK in component space. The elbow keeps the side it already has in the animation (its offset from
	 * the shoulder-to-target line), so no pole target is needed. The target is clamped to the reach of the arm.
	 */
	bool SolveTwoBone(const FTransform& Upper, const FTransform& Lower, const FTransform& Hand, const FVector& Target, FIKResult& Out)
	{
		const FVector A = Upper.GetLocation();
		const FVector B = Lower.GetLocation();
		const FVector C = Hand.GetLocation();

		const double L1 = (B - A).Size();
		const double L2 = (C - B).Size();
		const FVector ToTarget = Target - A;
		const double RawDist = ToTarget.Size();
		if (L1 < KINDA_SMALL_NUMBER || L2 < KINDA_SMALL_NUMBER || RawDist < KINDA_SMALL_NUMBER)
		{
			return false;
		}

		const FVector Dir = ToTarget / RawDist;
		const double Dist = FMath::Clamp(RawDist, FMath::Abs(L1 - L2) + 0.1, L1 + L2 - 0.1);

		// Keep the animated bend direction: the part of the current elbow offset that is perpendicular to the reach line.
		FVector Pole = (B - A) - Dir * FVector::DotProduct(B - A, Dir);
		if (Pole.SizeSquared() < 1e-4)
		{
			// Arm is stretched straight in the animation: bend downwards (component -Z made perpendicular to the reach line).
			Pole = FVector(0.0, 0.0, -1.0) - Dir * FVector::DotProduct(FVector(0.0, 0.0, -1.0), Dir);
			if (Pole.SizeSquared() < 1e-4)
			{
				Pole = FVector(0.0, 1.0, 0.0);
			}
		}
		Pole.Normalize();

		const double X = (L1 * L1 - L2 * L2 + Dist * Dist) / (2.0 * Dist);
		const double H = FMath::Sqrt(FMath::Max(L1 * L1 - X * X, 0.0));
		const FVector Elbow = A + Dir * X + Pole * H;
		const FVector NewHand = A + Dir * Dist;

		const FQuat R1 = FQuat::FindBetweenNormals((B - A).GetSafeNormal(), (Elbow - A).GetSafeNormal());
		const FVector LowerVecAfterR1 = R1.RotateVector(C - B);
		const FQuat R2 = FQuat::FindBetweenNormals(LowerVecAfterR1.GetSafeNormal(), (NewHand - Elbow).GetSafeNormal());

		Out.Upper = Upper;
		Out.Upper.SetRotation((R1 * Upper.GetRotation()).GetNormalized());

		Out.Lower = Lower;
		Out.Lower.SetLocation(Elbow);
		Out.Lower.SetRotation((R2 * R1 * Lower.GetRotation()).GetNormalized());

		Out.Hand = Hand;
		Out.Hand.SetLocation(NewHand);
		Out.Hand.SetRotation((R2 * R1 * Hand.GetRotation()).GetNormalized());
		return true;
	}
}

UBodycamHandGripComponent::UBodycamHandGripComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	LeftHand.bEnabled = true;
	LeftHand.UpperArmBone = TEXT("upperarm_l");
	LeftHand.LowerArmBone = TEXT("lowerarm_l");
	LeftHand.HandBone = TEXT("hand_l");
	// First guess under the AKM handguard (weapon space: X forward, Z up, origin at the top of the pistol grip). Tune with DebugDraw.
	LeftHand.GripInWeaponSpace = FTransform(FRotator::ZeroRotator, FVector(32.0, 0.0, 2.0));

	RightHand.bEnabled = false;
	RightHand.UpperArmBone = TEXT("upperarm_r");
	RightHand.LowerArmBone = TEXT("lowerarm_r");
	RightHand.HandBone = TEXT("hand_r");
	RightHand.GripInWeaponSpace = FTransform(FRotator::ZeroRotator, FVector(0.0, 0.0, -2.0));
}

void UBodycamHandGripComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (!Arms || !Weapon)
	{
		TArray<USceneComponent*> SceneComponents;
		Owner->GetComponents(SceneComponents);
		for (USceneComponent* Component : SceneComponents)
		{
			if (!Component)
			{
				continue;
			}
			if (!Arms && Component->GetFName() == ArmsComponentName)
			{
				Arms = Cast<USkeletalMeshComponent>(Component);
			}
			else if (!Weapon && Component->GetFName() == WeaponComponentName)
			{
				Weapon = Component;
			}
		}
	}

	Bind();
}

void UBodycamHandGripComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Unbind();
	Super::EndPlay(EndPlayReason);
}

void UBodycamHandGripComponent::SetMeshes(USkeletalMeshComponent* NewArms, USceneComponent* NewWeapon)
{
	Unbind();
	Arms = NewArms;
	Weapon = NewWeapon;
	bWarned = false;
	Bind();
}

void UBodycamHandGripComponent::Bind()
{
	if (!Arms || !Weapon)
	{
		UE_LOG(LogBodycam, Warning, TEXT("HandGrip on %s: arms ('%s') or weapon ('%s') component not found."),
			*GetNameSafe(GetOwner()), *ArmsComponentName.ToString(), *WeaponComponentName.ToString());
		return;
	}

	BuildChain(LeftHand, LeftChain);
	BuildChain(RightHand, RightChain);
	BoneHandle = Arms->RegisterOnBoneTransformsFinalizedDelegate(
		FOnBoneTransformsFinalized::CreateUObject(this, &UBodycamHandGripComponent::OnBonesFinalized));
}

void UBodycamHandGripComponent::Unbind()
{
	if (Arms && BoneHandle.IsValid())
	{
		Arms->UnregisterOnBoneTransformsFinalizedDelegate(BoneHandle);
	}
	BoneHandle.Reset();
}

void UBodycamHandGripComponent::BuildChain(const FBodycamHandGrip& Grip, FChain& OutChain) const
{
	OutChain = FChain();

	const USkeletalMesh* Mesh = Arms ? Arms->GetSkeletalMeshAsset() : nullptr;
	if (!Mesh)
	{
		return;
	}

	const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
	OutChain.Bones[0] = Ref.FindBoneIndex(Grip.UpperArmBone);
	OutChain.Bones[1] = Ref.FindBoneIndex(Grip.LowerArmBone);
	OutChain.Bones[2] = Ref.FindBoneIndex(Grip.HandBone);
	for (int32 Bone : OutChain.Bones)
	{
		if (Bone == INDEX_NONE)
		{
			if (Grip.bEnabled)
			{
				UE_LOG(LogBodycam, Warning, TEXT("HandGrip on %s: bone chain %s / %s / %s not found on %s."),
					*GetNameSafe(GetOwner()), *Grip.UpperArmBone.ToString(), *Grip.LowerArmBone.ToString(), *Grip.HandBone.ToString(), *GetNameSafe(Mesh));
			}
			return;
		}
	}

	// Parents always come before children in a reference skeleton, so one forward pass is enough.
	const int32 NumBones = Ref.GetNum();
	OutChain.Driver.Init(-1, NumBones);
	for (int32 i = 0; i < NumBones; ++i)
	{
		int8 Slot = -1;
		for (int32 k = 0; k < 3; ++k)
		{
			if (i == OutChain.Bones[k])
			{
				Slot = static_cast<int8>(k);
			}
		}
		if (Slot < 0)
		{
			const int32 Parent = Ref.GetParentIndex(i);
			Slot = Parent >= 0 ? OutChain.Driver[Parent] : static_cast<int8>(-1);
		}
		OutChain.Driver[i] = Slot;
	}
	OutChain.bValid = true;
}

bool UBodycamHandGripComponent::ComputeWeaponInArms(const TArray<FTransform>& ComponentSpace, FTransform& OutWeaponInArms) const
{
	const FName Socket = Weapon->GetAttachSocketName();
	if (Weapon->GetAttachParent() != Arms || Socket.IsNone())
	{
		OutWeaponInArms = Weapon->GetRelativeTransform();
		return Weapon->GetAttachParent() == Arms;
	}

	// The weapon hangs on a bone or a skeleton socket: rebuild that frame from THIS frame's bones, the component
	// transform of the weapon is still last frame's at this point.
	int32 BoneIndex = Arms->GetBoneIndex(Socket);
	FTransform SocketLocal = FTransform::Identity;
	if (BoneIndex == INDEX_NONE)
	{
		const USkeletalMeshSocket* SkeletalSocket = Arms->GetSocketByName(Socket);
		if (!SkeletalSocket)
		{
			return false;
		}
		BoneIndex = Arms->GetBoneIndex(SkeletalSocket->BoneName);
		SocketLocal = SkeletalSocket->GetSocketLocalTransform();
	}
	if (!ComponentSpace.IsValidIndex(BoneIndex))
	{
		return false;
	}

	OutWeaponInArms = Weapon->GetRelativeTransform() * SocketLocal * ComponentSpace[BoneIndex];
	return true;
}

void UBodycamHandGripComponent::OnBonesFinalized()
{
	if (!Arms || !Weapon || Alpha <= 0.f)
	{
		return;
	}

	TArray<FTransform>& ComponentSpace = Arms->GetEditableComponentSpaceTransforms();
	FTransform WeaponInArms;
	if (ComponentSpace.Num() == 0 || !ComputeWeaponInArms(ComponentSpace, WeaponInArms))
	{
		if (!bWarned)
		{
			UE_LOG(LogBodycam, Warning, TEXT("HandGrip on %s: weapon must be attached to the arms mesh (bone or socket)."), *GetNameSafe(GetOwner()));
			bWarned = true;
		}
		return;
	}

	SolveHand(LeftHand, LeftChain, ComponentSpace, WeaponInArms);
	SolveHand(RightHand, RightChain, ComponentSpace, WeaponInArms);
}

void UBodycamHandGripComponent::SolveHand(const FBodycamHandGrip& Grip, FChain& Chain, TArray<FTransform>& ComponentSpace, const FTransform& WeaponInArms)
{
	if (!Grip.bEnabled || !Chain.bValid || Chain.Driver.Num() != ComponentSpace.Num())
	{
		return;
	}

	const FTransform Old[3] = {
		ComponentSpace[Chain.Bones[0]],
		ComponentSpace[Chain.Bones[1]],
		ComponentSpace[Chain.Bones[2]],
	};

	const FTransform GripTarget = Grip.GripInWeaponSpace * WeaponInArms;
	const FVector Target = FMath::Lerp(Old[2].GetLocation(), GripTarget.GetLocation(), static_cast<double>(Alpha));

	FIKResult Result;
	if (!SolveTwoBone(Old[0], Old[1], Old[2], Target, Result))
	{
		return;
	}

	const double RotationBlend = Grip.RotationWeight * Alpha;
	if (RotationBlend > 0.0)
	{
		Result.Hand.SetRotation(FQuat::Slerp(Result.Hand.GetRotation(), GripTarget.GetRotation(), RotationBlend).GetNormalized());
	}
	const FTransform New[3] = { Result.Upper, Result.Lower, Result.Hand };

	// Carry everything below the arm (twist bones, fingers) rigidly with the bone it hangs on.
	for (int32 i = 0; i < ComponentSpace.Num(); ++i)
	{
		const int32 Slot = Chain.Driver[i];
		if (Slot >= 0)
		{
			ComponentSpace[i] = ComponentSpace[i].GetRelativeTransform(Old[Slot]) * New[Slot];
		}
	}
	for (int32 k = 0; k < 3; ++k)
	{
		ComponentSpace[Chain.Bones[k]] = New[k];
	}
}

FTransform UBodycamHandGripComponent::GetGripWorldTransform(bool bLeftHand) const
{
	const FBodycamHandGrip& Grip = bLeftHand ? LeftHand : RightHand;
	return Weapon ? Grip.GripInWeaponSpace * Weapon->GetComponentTransform() : FTransform::Identity;
}

void UBodycamHandGripComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bDebugDraw || !Arms || !Weapon)
	{
		return;
	}

	for (const bool bLeft : { true, false })
	{
		const FBodycamHandGrip& Grip = bLeft ? LeftHand : RightHand;
		if (!Grip.bEnabled)
		{
			continue;
		}
		const FTransform World = GetGripWorldTransform(bLeft);
		const FColor Color = bLeft ? FColor::Blue : FColor::Red;
		DrawDebugSphere(GetWorld(), World.GetLocation(), 2.f, 8, Color, false, 0.f);
		DrawDebugCoordinateSystem(GetWorld(), World.GetLocation(), World.Rotator(), 6.f, false, 0.f, 0, 0.3f);
		DrawDebugLine(GetWorld(), World.GetLocation(), Arms->GetBoneLocation(Grip.HandBone), Color, false, 0.f, 0, 0.2f);
	}
}
