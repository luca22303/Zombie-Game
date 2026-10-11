#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "BodycamProfile.h"
#include "BodycamSpring.h"
#include "BodycamCameraComponent.generated.h"

class UBodycamOSDWidget;
class UMaterialInstanceDynamic;
class APawn;

/**
 * Drop-in replacement for the first-person camera that behaves like a chest-mounted body camera.
 *
 * Setup: attach it to the character's capsule at chest height (about 130 cm above the feet), assign a
 * BodycamProfile and make sure it is the active camera. The component does not use the pawn control rotation;
 * it follows it through a spring, so the picture trails the aim and wobbles back.
 *
 * Use GetAimRotation()/GetAimLocation() for shooting and interaction traces so camera shake never affects aim.
 */
UCLASS(ClassGroup = (Camera), meta = (BlueprintSpawnableComponent))
class BODYCAMCAMERA_API UBodycamCameraComponent : public UCameraComponent
{
	GENERATED_BODY()

public:
	UBodycamCameraComponent();

	/** All tuning values. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam")
	TObjectPtr<UBodycamProfile> Profile;

	/** Optional widget Blueprint (parent: BodycamOSDWidget) created for the local player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|OSD")
	TSubclassOf<UBodycamOSDWidget> OSDWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam|OSD")
	int32 OSDZOrder = 0;

	/** Scales all camera motion (bob, lag, sway, impulses). 0 = rock-steady camera, 1 = as authored. Expose this in the options menu. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bodycam", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float MotionIntensity = 1.f;

	/** Scales the lens effects (distortion, rolling shutter, grain, ...). 0 = clean image. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bodycam", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LensIntensity = 1.f;

	UFUNCTION(BlueprintCallable, Category = "Bodycam")
	void SetProfile(UBodycamProfile* NewProfile);

	UFUNCTION(BlueprintCallable, Category = "Bodycam")
	void SetMotionIntensity(float NewIntensity);

	UFUNCTION(BlueprintCallable, Category = "Bodycam")
	void SetLensIntensity(float NewIntensity);

	/** Kick the camera. Position impulse in cm/s (camera-parent space: X forward, Y right, Z up), rotation impulse in deg/s. */
	UFUNCTION(BlueprintCallable, Category = "Bodycam")
	void AddCameraImpulse(FVector PositionImpulse, FRotator RotationImpulse);

	/** Damage / zombie grab reaction. WorldDirection is the direction the hit travels in. Strength ~ 1 for a normal hit. */
	UFUNCTION(BlueprintCallable, Category = "Bodycam")
	void AddHitReaction(FVector WorldDirection, float Strength = 1.f);

	/** Weapon fire: PitchKick/YawKick in deg/s (positive pitch kicks the view up), PushBack in cm/s. */
	UFUNCTION(BlueprintCallable, Category = "Bodycam")
	void AddRecoil(float PitchKick = 40.f, float YawKick = 0.f, float PushBack = 60.f);

	/** Clear all spring state and snap to the current aim (use after teleport / respawn). */
	UFUNCTION(BlueprintCallable, Category = "Bodycam")
	void ResetState();

	/** Where the player is really aiming (control rotation, no camera shake). Use for traces and projectile spawns. */
	UFUNCTION(BlueprintPure, Category = "Bodycam")
	FRotator GetAimRotation() const;

	/** Camera position without bob/shake offsets. */
	UFUNCTION(BlueprintPure, Category = "Bodycam")
	FVector GetAimLocation() const;

	/** Camera angular velocity in deg/s (pitch, yaw, roll) in camera space. */
	UFUNCTION(BlueprintPure, Category = "Bodycam")
	FRotator GetViewAngularVelocity() const { return ViewAngularVelocity; }

	/** Smoothed horizontal acceleration of the owner in cm/s^2, in the attach parent's space. */
	UFUNCTION(BlueprintPure, Category = "Bodycam")
	FVector GetLocalAcceleration() const { return LocalAcceleration; }

	UFUNCTION(BlueprintPure, Category = "Bodycam")
	UBodycamOSDWidget* GetOSDWidget() const;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void ApplyProfile();
	void ApplyLensSettings();
	void UpdateLensParameters(float DeltaTime);
	void TryCreateOSD(const APawn* Pawn);
	void UpdateViewportSize();
	FQuat GetAimQuat() const;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LensMID;

	UPROPERTY(Transient)
	TObjectPtr<UBodycamOSDWidget> OSDWidget;

	// Spring state
	FBodycamSpringQuat LagSpring;
	FBodycamSpringVector PositionSpring;  // cm, attach-parent space
	FBodycamSpringVector RotationSpring;  // degrees (X = pitch, Y = yaw, Z = roll)
	FBodycamSpringVector StanceSpring;    // cm, Z only, smooths crouching

	// Locomotion state
	FBodycamMotionSettings CurrentMotion;
	FVector BaseRelativeLocation = FVector::ZeroVector;
	FVector PreviousVelocity = FVector::ZeroVector;
	FVector SmoothedAcceleration = FVector::ZeroVector;
	FVector LocalAcceleration = FVector::ZeroVector;
	FRotator ViewAngularVelocity = FRotator::ZeroRotator;
	FQuat PreviousFinalRotation = FQuat::Identity;
	FRandomStream Random;
	double StrideAccumulator = 0.0;
	double LastAirborneVelocityZ = 0.0;
	double ElapsedTime = 0.0;
	float LastCapsuleHalfHeight = -1.f;
	float MoveWeight = 0.f;
	float WorldYawRate = 0.f;
	float SmoothedYawRate = 0.f;
	int32 LastStepIndex = 0;
	FVector2D LastViewportSize = FVector2D::ZeroVector;
	bool bInitialized = false;
	bool bHasPreviousVelocity = false;
	bool bHasPreviousFinal = false;
	bool bWasFalling = false;
};
