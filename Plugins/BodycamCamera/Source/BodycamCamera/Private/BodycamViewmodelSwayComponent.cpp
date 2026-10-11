#include "BodycamViewmodelSwayComponent.h"

#include "BodycamCameraComponent.h"
#include "BodycamLog.h"
#include "GameFramework/Actor.h"

UBodycamViewmodelSwayComponent::UBodycamViewmodelSwayComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UBodycamViewmodelSwayComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	Camera = Owner ? Owner->FindComponentByClass<UBodycamCameraComponent>() : nullptr;
	if (!Camera)
	{
		UE_LOG(LogBodycam, Warning, TEXT("ViewmodelSway on %s: no BodycamCameraComponent found."), *GetNameSafe(Owner));
		return;
	}

	// Run after the camera so we always read this frame's angular velocity.
	AddTickPrerequisiteComponent(Camera);

	if (!Target && !TargetComponentName.IsNone() && Owner)
	{
		TArray<USceneComponent*> SceneComponents;
		Owner->GetComponents(SceneComponents);
		for (USceneComponent* Component : SceneComponents)
		{
			if (Component && Component->GetFName() == TargetComponentName)
			{
				Target = Component;
				break;
			}
		}
	}

	CaptureBaseTransform();
}

void UBodycamViewmodelSwayComponent::SetTarget(USceneComponent* NewTarget)
{
	Target = NewTarget;
	CaptureBaseTransform();
}

void UBodycamViewmodelSwayComponent::CaptureBaseTransform()
{
	bHasBase = false;
	if (Target)
	{
		BaseLocation = Target->GetRelativeLocation();
		BaseRotation = Target->GetRelativeRotation().Quaternion();
		bHasBase = true;
	}
}

void UBodycamViewmodelSwayComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Camera || !Target || !bHasBase || DeltaTime <= 0.f)
	{
		return;
	}

	const double Dt = FMath::Min(DeltaTime, 0.1f);
	const double Intensity = Camera->MotionIntensity;

	const FRotator Turn = Camera->GetViewAngularVelocity();
	const FVector Accel = Camera->GetLocalAcceleration();

	const double MaxRot = MaxRotation;
	const double MaxPos = MaxPosition;

	// Viewmodel trails behind the view, so targets oppose the motion.
	const FVector RotationTarget(
		FMath::Clamp<double>(-Turn.Pitch * RotationPerTurnRate, -MaxRot, MaxRot),
		FMath::Clamp<double>(-Turn.Yaw * RotationPerTurnRate, -MaxRot, MaxRot),
		FMath::Clamp<double>(-Turn.Yaw * RollPerYawRate, -MaxRot, MaxRot));

	const FVector PositionTarget(
		-Accel.X * 0.001 * PositionPerAcceleration,
		FMath::Clamp<double>(-Turn.Yaw * PositionPerTurnRate, -MaxPos, MaxPos) - Accel.Y * 0.001 * PositionPerAcceleration,
		FMath::Clamp<double>(-Turn.Pitch * PositionPerTurnRate, -MaxPos, MaxPos));

	RotationSpring.Step(RotationTarget * Intensity, Dt, Frequency, Damping);
	PositionSpring.Step(PositionTarget.GetClampedToMaxSize(MaxPosition) * Intensity, Dt, Frequency, Damping);

	const FRotator Offset(RotationSpring.Position.X, RotationSpring.Position.Y, RotationSpring.Position.Z);
	Target->SetRelativeLocationAndRotation(BaseLocation + PositionSpring.Position, BaseRotation * Offset.Quaternion());
}
