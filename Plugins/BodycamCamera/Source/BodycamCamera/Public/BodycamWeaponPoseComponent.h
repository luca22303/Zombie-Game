#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "BodycamSpring.h"
#include "BodycamWeaponPoseComponent.generated.h"

class UBodycamCameraComponent;
class USceneComponent;

/**
 * Sprint "low ready" lock and weapon-vs-world collision for the first-person viewmodel.
 *
 * Both effects move one scene component, the PIVOT, which sits between the bodycam camera and the viewmodel:
 *
 *   BodycamCamera > WeaponPivot (plain SceneComponent, identity transform) > ViewmodelRoot > ViewmodelArms > Weapon
 *
 * This component is the only writer of the pivot's relative transform, so it does not fight the sway component
 * (writes ViewmodelRoot) or the weapon stance component (writes the arms). Because the arms and the weapon are
 * children of the pivot, hands and weapon always move as one piece.
 *
 * 1. SPRINT LOCK. While sprinting, the weapon drops into a low-ready pose and is held in a body-stable frame
 *    (view yaw, aim pitch, no camera bob). The weapon follows the view pitch only while you look at or below
 *    LockPitch; looking higher, the weapon stays at LockPitch, so it sinks out of the bottom of the frame and
 *    the view is clear. Because the weapon is held in the stable frame it no longer bobs with the camera.
 *
 * 2. COLLISION. Sphere sweeps run from the eye to the muzzle (a centre probe and four probes spread around the
 *    muzzle). The deepest penetration pulls the weapon back, raises or lowers the muzzle and yaws it away
 *    from side walls, through springs, so the gun never ends up inside geometry. IsWeaponBlocked() tells the
 *    weapon code to refuse to fire.
 */
UCLASS(ClassGroup = (Camera), meta = (BlueprintSpawnableComponent))
class BODYCAMCAMERA_API UBodycamWeaponPoseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBodycamWeaponPoseComponent();

	/** Name of the pivot scene component (child of the bodycam camera, parent of the viewmodel). Ignored if SetTarget was called. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose")
	FName TargetComponentName = TEXT("WeaponPivot");

	// ----------------------------------------------------------------------------------------------------------------
	// Sprint lock
	// ----------------------------------------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sprint Lock")
	bool bEnableSprintLock = true;

	/** Ground speed (cm/s) at which the lock engages. Put it between your walk and sprint speeds (walk 250, sprint 450 -> 350). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sprint Lock", meta = (ClampMin = "0.0", Units = "cm/s"))
	float SprintSpeedThreshold = 350.f;

	/** The lock releases below SprintSpeedThreshold * this (hysteresis, stops flicker around the threshold). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sprint Lock", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float SprintReleaseRatio = 0.9f;

	/** Seconds to go from ready to low ready. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sprint Lock", meta = (ClampMin = "0.01", Units = "s"))
	float LockBlendInTime = 0.18f;

	/** Seconds to come back to ready. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sprint Lock", meta = (ClampMin = "0.01", Units = "s"))
	float LockBlendOutTime = 0.25f;

	/**
	 * View pitch in degrees (negative = looking down) at and below which the weapon follows the view.
	 * Above it the weapon stays at this pitch and drops out of the frame. About -10 to -20 feels right.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sprint Lock", meta = (ClampMin = "-89.0", ClampMax = "89.0", Units = "deg"))
	float LockPitch = -15.f;

	/** Degrees over which the lock fades in around LockPitch (0 = hard corner, 5 to 10 = smooth). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sprint Lock", meta = (ClampMin = "0.0", Units = "deg"))
	float LockSoftness = 8.f;

	/** Low-ready position offset in cm, in the stable frame (X forward, Y right, Z up). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sprint Lock")
	FVector SprintOffset = FVector(4.f, 3.f, -22.f);

	/** Low-ready rotation: negative pitch points the muzzle down, roll tips the weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sprint Lock")
	FRotator SprintRotation = FRotator(-38.f, 10.f, -14.f);

	// ----------------------------------------------------------------------------------------------------------------
	// Collision
	// ----------------------------------------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	bool bEnableCollision = true;

	/** Channel the probes trace against. Walls and props must block it (Visibility works for most projects). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Distance in cm from the eye to the muzzle (AKM: about 38 cm eye relief - 15 cm rear sight + 70 cm barrel). Measure with the debug draw. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (ClampMin = "10.0", Units = "cm"))
	float WeaponReach = 95.f;

	/** Radius of the probe spheres in cm (roughly half the weapon's thickness). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (ClampMin = "0.5", Units = "cm"))
	float ProbeRadius = 5.f;

	/** How far from the weapon axis the four side probes sit at the muzzle, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (ClampMin = "0.0", Units = "cm"))
	float ProbeSpread = 10.f;

	/** cm the weapon is pulled back per cm of penetration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (ClampMin = "0.0"))
	float PushBackRatio = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxPushBack = 30.f;

	/** Degrees the muzzle is tilted per cm of penetration (up when blocked in front or below, down when blocked above). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (ClampMin = "0.0", Units = "deg"))
	float TiltPerCm = 1.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (ClampMin = "0.0", Units = "deg"))
	float MaxTilt = 40.f;

	/** Degrees the muzzle is yawed away from a side wall per cm of side penetration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (ClampMin = "0.0", Units = "deg"))
	float YawPerCm = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (ClampMin = "0.0", Units = "deg"))
	float MaxYaw = 25.f;

	/** Penetration in cm above which IsWeaponBlocked() is true (use it to block firing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (ClampMin = "0.0", Units = "cm"))
	float BlockedThreshold = 12.f;

	/** Spring frequency (Hz) while the weapon is moving INTO a pose (fast, so it never clips for long). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (ClampMin = "1.0", ClampMax = "20.0"))
	float PushFrequency = 14.f;

	/** Spring frequency (Hz) while the weapon returns to rest (slower, so it eases back). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (ClampMin = "1.0", ClampMax = "20.0"))
	float ReleaseFrequency = 7.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision", meta = (ClampMin = "0.2", ClampMax = "2.0"))
	float CollisionDamping = 0.9f;

	/** Draw the probes (green = free, red = hit) and the muzzle point. Use it to measure WeaponReach. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDebugDraw = false;

	// ----------------------------------------------------------------------------------------------------------------
	// Runtime API
	// ----------------------------------------------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Pose")
	void SetTarget(USceneComponent* NewPivot);

	/** Call with true while aiming down sights or firing: the sprint lock releases and cannot engage. Collision stays on. */
	UFUNCTION(BlueprintCallable, Category = "Pose")
	void SetSprintLockSuppressed(bool bSuppressed);

	/** 0..1, how far the weapon is in low ready (use it to block firing / aiming or to drive the legs). */
	UFUNCTION(BlueprintPure, Category = "Pose")
	float GetSprintAlpha() const { return SprintAlpha; }

	UFUNCTION(BlueprintPure, Category = "Pose")
	bool IsSprintLocked() const { return bSprintLatched; }

	/** Deepest probe penetration in cm after smoothing (0 = free). */
	UFUNCTION(BlueprintPure, Category = "Pose")
	float GetPenetration() const { return SmoothedPenetration; }

	/** True when the muzzle is buried deeper than BlockedThreshold: do not allow firing. */
	UFUNCTION(BlueprintPure, Category = "Pose")
	bool IsWeaponBlocked() const { return SmoothedPenetration > BlockedThreshold; }

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void FindTarget();
	void UpdateSprint(float DeltaTime);

	/** Frame the weapon would have without collision (camera frame blended into the stable low-ready frame). */
	FTransform ComputeBaseFrame() const;

	/** Runs the probes from the eye along the base frame. Returns penetration, tilt (deg, + = muzzle up) and yaw (deg, + = muzzle right). */
	void RunProbes(const FVector& Eye, const FTransform& BaseFrame, float& OutPenetration, float& OutTilt, float& OutYaw) const;

	UPROPERTY(Transient)
	TObjectPtr<UBodycamCameraComponent> Camera;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Pivot;

	FBodycamSpringVector PushSpring;  // X = push back (cm), Y = tilt (deg), Z = yaw (deg)
	float SprintAlpha = 0.f;
	float SmoothedPenetration = 0.f;
	bool bSprintLatched = false;
	bool bSuppressed = false;
	bool bWarnedNoPivot = false;
};
