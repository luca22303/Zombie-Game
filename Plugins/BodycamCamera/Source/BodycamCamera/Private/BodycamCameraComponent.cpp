#include "BodycamCameraComponent.h"

#include "BodycamLog.h"
#include "BodycamOSDWidget.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"

using namespace BodycamMath;

static TAutoConsoleVariable<float> CVarBodycamMotionScale(
	TEXT("bodycam.MotionScale"), 1.0f,
	TEXT("Global multiplier on all bodycam camera motion (on top of the component's MotionIntensity)."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarBodycamDebug(
	TEXT("bodycam.Debug"), 0,
	TEXT("1 = print bodycam camera state on screen."),
	ECVF_Default);

namespace
{
	const FName P_Distortion(TEXT("Distortion"));
	const FName P_RollingShutter(TEXT("RollingShutter"));
	const FName P_YawRate(TEXT("YawRate"));
	const FName P_Compression(TEXT("Compression"));
	const FName P_SensorNoise(TEXT("SensorNoise"));
	const FName P_Aspect(TEXT("Aspect"));
	const FName P_ViewWidth(TEXT("ViewWidth"));
	const FName P_ViewHeight(TEXT("ViewHeight"));
}

UBodycamCameraComponent::UBodycamCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	// The component computes its own rotation from the (lagged) control rotation.
	bUsePawnControlRotation = false;

	Random.Initialize(1337);
}

void UBodycamCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	BaseRelativeLocation = GetRelativeLocation();
	ApplyProfile();
}

void UBodycamCameraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (OSDWidget)
	{
		OSDWidget->RemoveFromParent();
		OSDWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------------------------------------------------

void UBodycamCameraComponent::SetProfile(UBodycamProfile* NewProfile)
{
	Profile = NewProfile;
	ApplyProfile();
}

void UBodycamCameraComponent::SetMotionIntensity(float NewIntensity)
{
	MotionIntensity = FMath::Clamp(NewIntensity, 0.f, 2.f);
}

void UBodycamCameraComponent::SetLensIntensity(float NewIntensity)
{
	LensIntensity = FMath::Clamp(NewIntensity, 0.f, 1.f);
	ApplyLensSettings();
}

void UBodycamCameraComponent::AddCameraImpulse(FVector PositionImpulse, FRotator RotationImpulse)
{
	PositionSpring.AddImpulse(PositionImpulse);
	RotationSpring.AddImpulse(FVector(RotationImpulse.Pitch, RotationImpulse.Yaw, RotationImpulse.Roll));
}

void UBodycamCameraComponent::AddHitReaction(FVector WorldDirection, float Strength)
{
	const USceneComponent* Parent = GetAttachParent();
	const FQuat ParentQuat = Parent ? Parent->GetComponentQuat() : FQuat::Identity;
	const FVector Local = ParentQuat.UnrotateVector(WorldDirection.GetSafeNormal());

	PositionSpring.AddImpulse(Local * 180.0 * Strength);
	RotationSpring.AddImpulse(FVector(
		-Local.X * 55.0 + Random.FRandRange(-15.f, 15.f),
		Random.FRandRange(-30.f, 30.f),
		Local.Y * 90.0 + Random.FRandRange(-20.f, 20.f)) * Strength);
}

void UBodycamCameraComponent::AddRecoil(float PitchKick, float YawKick, float PushBack)
{
	RotationSpring.AddImpulse(FVector(PitchKick, YawKick, 0.0));
	PositionSpring.AddImpulse(FVector(-PushBack, 0.0, 0.0));
}

void UBodycamCameraComponent::ResetState()
{
	PositionSpring.Reset();
	RotationSpring.Reset();
	StanceSpring.Reset();
	LagSpring.Snap(GetAimQuat());
	SmoothedAcceleration = FVector::ZeroVector;
	LocalAcceleration = FVector::ZeroVector;
	bHasPreviousVelocity = false;
	bHasPreviousFinal = false;
	LastCapsuleHalfHeight = -1.f;
	bInitialized = false;
}

UBodycamOSDWidget* UBodycamCameraComponent::GetOSDWidget() const
{
	return OSDWidget;
}

FRotator UBodycamCameraComponent::GetAimRotation() const
{
	return GetAimQuat().Rotator();
}

FVector UBodycamCameraComponent::GetAimLocation() const
{
	const USceneComponent* Parent = GetAttachParent();
	return Parent
		? Parent->GetComponentTransform().TransformPositionNoScale(BaseRelativeLocation)
		: GetComponentLocation();
}

// ---------------------------------------------------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------------------------------------------------

FQuat UBodycamCameraComponent::GetAimQuat() const
{
	const AActor* Owner = GetOwner();
	if (const APawn* Pawn = Cast<APawn>(Owner))
	{
		return Pawn->GetViewRotation().Quaternion();
	}
	return Owner ? Owner->GetActorQuat() : FQuat::Identity;
}

void UBodycamCameraComponent::ApplyProfile()
{
	if (LensMID)
	{
		PostProcessSettings.RemoveBlendable(LensMID.Get());
		LensMID = nullptr;
	}

	if (!Profile)
	{
		UE_LOG(LogBodycam, Warning, TEXT("%s: no BodycamProfile assigned."), *GetNameSafe(GetOwner()));
		return;
	}

	CurrentMotion = Profile->Walk;
	LastViewportSize = FVector2D::ZeroVector;

	if (Profile->LensMaterial)
	{
		LensMID = UMaterialInstanceDynamic::Create(Profile->LensMaterial, this);
		if (LensMID)
		{
			PostProcessSettings.AddBlendable(LensMID.Get(), 1.f);
		}
	}

	ApplyLensSettings();
}

void UBodycamCameraComponent::ApplyLensSettings()
{
	if (!Profile)
	{
		return;
	}

	const FBodycamLensSettings& L = Profile->Lens;
	const float K = FMath::Clamp(LensIntensity, 0.f, 1.f);

	SetFieldOfView(L.FieldOfView);

	FPostProcessSettings& PP = PostProcessSettings;

	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = L.Vignette * K;

	PP.bOverride_FilmGrainIntensity = true;
	PP.FilmGrainIntensity = L.FilmGrain * K;

	PP.bOverride_SceneFringeIntensity = true;
	PP.SceneFringeIntensity = L.ChromaticAberration * K;

	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = L.BloomIntensity;

	PP.bOverride_BloomDirtMask = true;
	PP.BloomDirtMask = L.LensDirtMask;
	PP.bOverride_BloomDirtMaskIntensity = true;
	PP.BloomDirtMaskIntensity = L.LensDirtMask ? L.LensDirtIntensity * K : 0.f;

	PP.bOverride_AutoExposureSpeedUp = true;
	PP.AutoExposureSpeedUp = L.ExposureSpeedUp;
	PP.bOverride_AutoExposureSpeedDown = true;
	PP.AutoExposureSpeedDown = L.ExposureSpeedDown;

	if (L.bOverrideExposureRange)
	{
		PP.bOverride_AutoExposureMinBrightness = true;
		PP.AutoExposureMinBrightness = L.ExposureMin;
		PP.bOverride_AutoExposureMaxBrightness = true;
		PP.AutoExposureMaxBrightness = L.ExposureMax;
	}
}

void UBodycamCameraComponent::TryCreateOSD(const APawn* Pawn)
{
	if (OSDWidget || !OSDWidgetClass || !Pawn)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	OSDWidget = CreateWidget<UBodycamOSDWidget>(PC, OSDWidgetClass);
	if (OSDWidget)
	{
		OSDWidget->AddToViewport(OSDZOrder);
	}
}

void UBodycamCameraComponent::UpdateViewportSize()
{
	const UWorld* World = GetWorld();
	const UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	if (!Viewport || !LensMID)
	{
		return;
	}

	FVector2D Size = FVector2D::ZeroVector;
	Viewport->GetViewportSize(Size);
	if (Size.X < 1.0 || Size.Y < 1.0 || Size == LastViewportSize)
	{
		return;
	}

	LastViewportSize = Size;
	LensMID->SetScalarParameterValue(P_ViewWidth, static_cast<float>(Size.X));
	LensMID->SetScalarParameterValue(P_ViewHeight, static_cast<float>(Size.Y));
	LensMID->SetScalarParameterValue(P_Aspect, static_cast<float>(Size.X / Size.Y));
}

void UBodycamCameraComponent::UpdateLensParameters(float DeltaTime)
{
	if (!LensMID || !Profile)
	{
		return;
	}

	const FBodycamLensSettings& L = Profile->Lens;
	const float K = FMath::Clamp(LensIntensity, 0.f, 1.f);

	SmoothedYawRate = FMath::FInterpTo(SmoothedYawRate, WorldYawRate, DeltaTime, 25.f);
	const float YawRateNormalized = FMath::Clamp(SmoothedYawRate / 360.f, -2.f, 2.f);
	const float MotionBoost = 1.f + FMath::Clamp(FMath::Abs(SmoothedYawRate) / 360.f, 0.f, 1.f);

	LensMID->SetScalarParameterValue(P_Distortion, L.Distortion * K);
	LensMID->SetScalarParameterValue(P_RollingShutter, L.RollingShutter * K);
	LensMID->SetScalarParameterValue(P_YawRate, YawRateNormalized);
	LensMID->SetScalarParameterValue(P_Compression, FMath::Clamp(L.Compression * K * MotionBoost, 0.f, 1.f));
	LensMID->SetScalarParameterValue(P_SensorNoise, L.SensorNoise * K);
}

// ---------------------------------------------------------------------------------------------------------------------
// Per-frame update
// ---------------------------------------------------------------------------------------------------------------------

void UBodycamCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	USceneComponent* Parent = GetAttachParent();
	if (!Owner || !Parent || !Profile || DeltaTime <= 0.f)
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(Owner);
	if (Pawn && !Pawn->IsLocallyControlled())
	{
		return;
	}

	const double Dt = FMath::Min(DeltaTime, 0.1f);
	const UBodycamProfile& P = *Profile;

	TryCreateOSD(Pawn);
	UpdateViewportSize();

	const double Intensity = FMath::Max(0.f, MotionIntensity) * FMath::Max(0.f, CVarBodycamMotionScale.GetValueOnGameThread());
	const FQuat ParentQuat = Parent->GetComponentQuat();
	ElapsedTime += Dt;

	// ---- Gameplay inputs ----------------------------------------------------------------------------------------
	const ACharacter* Character = Cast<ACharacter>(Owner);
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const FVector Velocity = Owner->GetVelocity();
	const double Speed2D = Velocity.Size2D();
	const bool bFalling = Movement && Movement->IsFalling();
	const bool bCrouched = Movement && Movement->IsCrouching();

	// Acceleration (horizontal only), smoothed.
	FVector RawAcceleration = FVector::ZeroVector;
	if (bHasPreviousVelocity)
	{
		RawAcceleration = (Velocity - PreviousVelocity) / Dt;
		RawAcceleration.Z = 0.0;
		RawAcceleration = RawAcceleration.GetClampedToMaxSize(P.MaxAcceleration);
	}
	PreviousVelocity = Velocity;
	bHasPreviousVelocity = true;
	SmoothedAcceleration = FMath::VInterpTo(SmoothedAcceleration, RawAcceleration, static_cast<float>(Dt), P.AccelerationSmoothing);
	LocalAcceleration = ParentQuat.UnrotateVector(SmoothedAcceleration);

	// ---- Locomotion state ---------------------------------------------------------------------------------------
	const FBodycamMotionSettings& TargetMotion = bCrouched
		? P.Crouch
		: (Speed2D >= P.SprintSpeedThreshold ? P.Sprint : P.Walk);
	CurrentMotion = FBodycamMotionSettings::Lerp(CurrentMotion, TargetMotion, 1.f - FMath::Exp(-P.MotionBlendRate * static_cast<float>(Dt)));

	const bool bMoving = !bFalling && Speed2D > P.MinMoveSpeed;
	const float MoveTarget = bMoving
		? static_cast<float>(FMath::Clamp(Speed2D / FMath::Max(CurrentMotion.ReferenceSpeed, 1.f), 0.0, 1.5))
		: 0.f;
	MoveWeight = FMath::FInterpTo(MoveWeight, MoveTarget, static_cast<float>(Dt), P.MoveWeightRate);

	// Stride phase advances with distance travelled, so the bob always matches the footsteps and stops with the player.
	if (bMoving)
	{
		StrideAccumulator += Speed2D * Dt / FMath::Max(CurrentMotion.StrideLength, 20.f);
	}
	const int32 StepIndex = FMath::FloorToInt(StrideAccumulator * 2.0);
	const bool bFootstep = bMoving && StepIndex > LastStepIndex;
	LastStepIndex = StepIndex;

	const double Phase = StrideAccumulator - FMath::FloorToDouble(StrideAccumulator);
	const double S1 = FMath::Sin(TwoPi * Phase);
	const double C1 = FMath::Cos(TwoPi * Phase);
	const double C2 = FMath::Cos(2.0 * TwoPi * Phase);
	const double S2 = FMath::Sin(2.0 * TwoPi * Phase);

	// ---- Spring targets ----------------------------------------------------------------------------------------
	const double Amp = MoveWeight * Intensity;
	const FBodycamMotionSettings& M = CurrentMotion;

	// Lowest at foot contact (Phase 0 and 0.5), highest mid-stride.
	const FVector BobPosition = FVector(M.ForwardBob * S2, M.LateralBob * S1, -M.VerticalBob * C2) * Amp;
	// X = pitch, Y = yaw, Z = roll
	const FVector BobRotation = FVector(-M.PitchBob * C2, M.YawBob * C1, M.RollBob * S1) * Amp;

	const FVector Breath(0.0, 0.0, FMath::Sin(TwoPi * P.BreathRate * ElapsedTime) * P.BreathAmplitude * Intensity);
	const double HandheldT = TwoPi * P.HandheldFrequency * ElapsedTime;
	const FVector Handheld(
		SmoothNoise(HandheldT, 1.3),
		SmoothNoise(HandheldT, 7.1),
		SmoothNoise(HandheldT, 12.9) * 0.5) * (P.HandheldAmplitude * Intensity);

	const double AccelScale = 1.0 / 1000.0;
	FVector AccelPosition(
		-LocalAcceleration.X * AccelScale * P.AccelerationOffset,
		-LocalAcceleration.Y * AccelScale * P.AccelerationOffset,
		0.0);
	AccelPosition = AccelPosition.GetClampedToMaxSize(P.MaxAccelerationOffset) * Intensity;
	const FVector AccelRotation = FVector(
		-LocalAcceleration.X * AccelScale * P.AccelerationPitch,
		0.0,
		LocalAcceleration.Y * AccelScale * P.AccelerationRoll) * Intensity;

	// ---- Impulses ----------------------------------------------------------------------------------------------
	if (bFootstep)
	{
		const double Side = (StepIndex & 1) ? 1.0 : -1.0;
		const double Strength = FMath::Clamp<double>(MoveWeight, 0.0, 1.5) * Intensity * Random.FRandRange(0.75f, 1.25f);
		PositionSpring.AddImpulse(FVector(0.0, Side * M.FootstepImpulse * 0.3, -M.FootstepImpulse) * Strength);
		RotationSpring.AddImpulse(FVector(
			-M.FootstepRotationImpulse * 0.6,
			Random.FRandRange(-0.5f, 0.5f) * M.FootstepRotationImpulse * 0.4,
			Side * M.FootstepRotationImpulse) * Strength);
	}

	if (bFalling)
	{
		LastAirborneVelocityZ = Velocity.Z;
	}

	if (bWasFalling && !bFalling)
	{
		const double FallSpeed = FMath::Max(0.0, -LastAirborneVelocityZ);
		const double Down = FMath::Min(FallSpeed * P.LandingImpulse, static_cast<double>(P.MaxLandingImpulse));
		const double PitchDown = FMath::Min(FallSpeed * P.LandingPitchImpulse, static_cast<double>(P.MaxLandingPitchImpulse));
		PositionSpring.AddImpulse(FVector(0.0, 0.0, -Down) * Intensity);
		RotationSpring.AddImpulse(FVector(-PitchDown, 0.0, Random.FRandRange(-1.f, 1.f) * PitchDown * 0.3) * Intensity);
	}
	else if (!bWasFalling && bFalling && Velocity.Z > 100.0)
	{
		PositionSpring.AddImpulse(FVector(0.0, 0.0, -P.JumpImpulse) * Intensity);
	}
	bWasFalling = bFalling;

	// ---- Crouch height smoothing -------------------------------------------------------------------------------
	if (P.bSmoothStanceChange && Character)
	{
		const float Half = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		if (LastCapsuleHalfHeight > 0.f && !FMath::IsNearlyEqual(Half, LastCapsuleHalfHeight))
		{
			// The capsule shrinks around its base, so the attached camera drops instantly. Cancel that and ease it in.
			StanceSpring.Position.Z += LastCapsuleHalfHeight - Half;
		}
		LastCapsuleHalfHeight = Half;
		StanceSpring.Step(FVector::ZeroVector, Dt, P.StanceFrequency, 1.0);
	}
	else
	{
		StanceSpring.Reset();
	}

	// ---- Integrate springs -------------------------------------------------------------------------------------
	PositionSpring.Step(BobPosition + Breath + AccelPosition, Dt, P.PositionFrequency, P.PositionDamping);
	RotationSpring.Step(BobRotation + Handheld + AccelRotation, Dt, P.RotationFrequency, P.RotationDamping);

	// Rotation lag: the camera chases the aim direction with overshoot.
	const FQuat AimQuat = GetAimQuat();
	if (!bInitialized || Intensity <= 0.0)
	{
		LagSpring.Snap(AimQuat);
		bInitialized = true;
	}
	else
	{
		const double LagFrequency = FMath::Lerp(25.0, static_cast<double>(P.RotationLagFrequency), FMath::Clamp(Intensity, 0.0, 1.0));
		LagSpring.Step(AimQuat, Dt, LagFrequency, P.RotationLagDamping);

		// Never trail the aim by more than MaxRotationLag degrees.
		FVector Error = QuatToRotationVector(AimQuat * LagSpring.Rotation.Inverse());
		const double MaxRadians = FMath::DegreesToRadians(static_cast<double>(P.MaxRotationLag));
		if (Error.Size() > MaxRadians)
		{
			Error = Error.GetSafeNormal() * MaxRadians;
			LagSpring.Rotation = RotationVectorToQuat(Error).Inverse() * AimQuat;
			LagSpring.Rotation.Normalize();
		}
	}

	// ---- Compose and apply -------------------------------------------------------------------------------------
	const FRotator OffsetRotator(
		RotationSpring.Position.X,
		RotationSpring.Position.Y,
		FMath::Clamp(RotationSpring.Position.Z, -static_cast<double>(P.MaxRoll), static_cast<double>(P.MaxRoll)));
	FQuat FinalRotation = LagSpring.Rotation * OffsetRotator.Quaternion();
	FinalRotation.Normalize();

	const FVector BaseLocation = Parent->GetComponentTransform().TransformPositionNoScale(BaseRelativeLocation);
	const FVector LocalOffset = PositionSpring.Position + StanceSpring.Position;
	const FVector FinalLocation = BaseLocation + ParentQuat.RotateVector(LocalOffset);

	SetWorldLocationAndRotation(FinalLocation, FinalRotation);

	// View angular velocity (used by the lens effect and the viewmodel sway).
	if (bHasPreviousFinal)
	{
		const FRotator Local = (PreviousFinalRotation.Inverse() * FinalRotation).Rotator().GetNormalized();
		ViewAngularVelocity = FRotator(Local.Pitch / Dt, Local.Yaw / Dt, Local.Roll / Dt);

		const FRotator World = (FinalRotation * PreviousFinalRotation.Inverse()).Rotator().GetNormalized();
		WorldYawRate = static_cast<float>(World.Yaw / Dt);
	}
	PreviousFinalRotation = FinalRotation;
	bHasPreviousFinal = true;

	UpdateLensParameters(static_cast<float>(Dt));

	if (CVarBodycamDebug.GetValueOnGameThread() != 0 && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			static_cast<int32>(GetUniqueID()), 0.f, FColor::Green,
			FString::Printf(TEXT("Bodycam  speed %.0f  move %.2f  phase %.2f  yawRate %.0f deg/s  accel (%.0f, %.0f)  pos (%.2f, %.2f, %.2f)"),
				Speed2D, MoveWeight, Phase, WorldYawRate, LocalAcceleration.X, LocalAcceleration.Y,
				PositionSpring.Position.X, PositionSpring.Position.Y, PositionSpring.Position.Z));
	}
}
