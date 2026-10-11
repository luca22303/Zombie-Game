#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BodycamSpring.h"
#include "BodycamViewmodelSwayComponent.generated.h"

class UBodycamCameraComponent;
class USceneComponent;

/**
 * Makes first-person arms / weapon float and lag inside the frame like they would on a chest mount.
 *
 * Attach the arms or weapon mesh to the BodycamCameraComponent, add this component to the character and point
 * it at that mesh (TargetComponentName, or SetTarget). It reads the camera's angular velocity and the character's
 * acceleration and drives the target's relative transform through its own spring, so the viewmodel trails the view.
 */
UCLASS(ClassGroup = (Camera), meta = (BlueprintSpawnableComponent))
class BODYCAMCAMERA_API UBodycamViewmodelSwayComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBodycamViewmodelSwayComponent();

	/** Name of the scene component (attached to the bodycam camera) to sway. Ignored if SetTarget was called. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sway")
	FName TargetComponentName;

	/** Degrees of viewmodel rotation lag per deg/s of camera turn rate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sway")
	float RotationPerTurnRate = 0.05f;

	/** Additional roll (deg) per deg/s of yaw rate: the weapon leans into turns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sway")
	float RollPerYawRate = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sway")
	float MaxRotation = 10.f;

	/** cm of viewmodel position lag per deg/s of camera turn rate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sway")
	float PositionPerTurnRate = 0.012f;

	/** cm of viewmodel position lag per 1000 cm/s^2 of character acceleration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sway")
	float PositionPerAcceleration = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sway")
	float MaxPosition = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sway", meta = (ClampMin = "0.5", ClampMax = "20.0"))
	float Frequency = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sway", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float Damping = 0.5f;

	UFUNCTION(BlueprintCallable, Category = "Sway")
	void SetTarget(USceneComponent* NewTarget);

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void CaptureBaseTransform();

	UPROPERTY(Transient)
	TObjectPtr<UBodycamCameraComponent> Camera;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Target;

	FBodycamSpringVector PositionSpring;
	FBodycamSpringVector RotationSpring;  // X = pitch, Y = yaw, Z = roll (degrees)
	FVector BaseLocation = FVector::ZeroVector;
	FQuat BaseRotation = FQuat::Identity;
	bool bHasBase = false;
};
