#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BodycamProfile.generated.h"

class UMaterialInterface;
class UTexture;

/** Procedural walking-motion amplitudes for one locomotion state (walk / sprint / crouch). */
USTRUCT(BlueprintType)
struct BODYCAMCAMERA_API FBodycamMotionSettings
{
	GENERATED_BODY()

	/** Ground speed (cm/s) at which the amplitudes below apply in full. Slower movement scales them down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (ClampMin = "1.0"))
	float ReferenceSpeed = 250.f;

	/** Distance (cm) covered by one full stride (two steps). Smaller = faster bobbing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (ClampMin = "20.0"))
	float StrideLength = 150.f;

	/** Up/down travel in cm (twice per stride). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	float VerticalBob = 1.0f;

	/** Side-to-side travel in cm (once per stride). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	float LateralBob = 0.8f;

	/** Forward/back travel in cm (twice per stride). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	float ForwardBob = 0.4f;

	/** Camera roll in degrees (once per stride). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	float RollBob = 0.6f;

	/** Camera pitch in degrees (twice per stride). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	float PitchBob = 0.5f;

	/** Camera yaw in degrees (once per stride, chest twist). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	float YawBob = 0.3f;

	/** Downward velocity kick (cm/s) applied on every footstep. This is what makes steps feel physical. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	float FootstepImpulse = 25.f;

	/** Rotational velocity kick (deg/s) applied on every footstep (alternating left/right roll). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	float FootstepRotationImpulse = 6.f;

	static FBodycamMotionSettings Lerp(const FBodycamMotionSettings& A, const FBodycamMotionSettings& B, float Alpha);
};

/** Everything that makes the picture look like it came from a body-worn camera. */
USTRUCT(BlueprintType)
struct BODYCAMCAMERA_API FBodycamLensSettings
{
	GENERATED_BODY()

	/** Wide-angle lens. Bodycams are typically 100-140 degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens", meta = (ClampMin = "60.0", ClampMax = "150.0"))
	float FieldOfView = 105.f;

	/** Barrel/fisheye strength (custom material). 0 = off. Crops the centre slightly; raise FOV to compensate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float Distortion = 0.35f;

	/** Rolling-shutter skew when turning fast (custom material). Flip the sign if the skew looks backwards. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float RollingShutter = 0.12f;

	/** Macroblock/banding compression artifacts, strongest in dark areas and fast motion (custom material). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Compression = 0.35f;

	/** Sensor noise in the shadows (custom material). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SensorNoise = 0.12f;

	/** Built-in vignette. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Vignette = 0.45f;

	/** Built-in film grain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FilmGrain = 0.25f;

	/** Built-in chromatic aberration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float ChromaticAberration = 1.2f;

	/** Built-in bloom intensity (needed for the lens dirt mask to show). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens", meta = (ClampMin = "0.0"))
	float BloomIntensity = 0.6f;

	/** Optional smudge/scratch texture used as bloom dirt mask. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens")
	TObjectPtr<UTexture> LensDirtMask = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens", meta = (ClampMin = "0.0"))
	float LensDirtIntensity = 4.f;

	/** Slow exposure adaptation gives the typical bodycam brightness "pump" when going from dark to bright. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens|Exposure", meta = (ClampMin = "0.01"))
	float ExposureSpeedUp = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens|Exposure", meta = (ClampMin = "0.01"))
	float ExposureSpeedDown = 0.7f;

	/** Also override the auto-exposure min/max range. Units depend on the project's exposure settings (EV100 in default UE5 projects). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens|Exposure")
	bool bOverrideExposureRange = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens|Exposure", meta = (EditCondition = "bOverrideExposureRange"))
	float ExposureMin = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens|Exposure", meta = (EditCondition = "bOverrideExposureRange"))
	float ExposureMax = 12.f;
};

/**
 * All tunable numbers of the bodycam camera. Create one via
 * Content Browser > Miscellaneous > Data Asset > BodycamProfile and assign it to the camera component.
 */
UCLASS(BlueprintType)
class BODYCAMCAMERA_API UBodycamProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UBodycamProfile();

	// ---- Rotation lag (the "mounted on the chest" feel) ----

	/** How quickly the camera catches up with where the player looks (Hz). Lower = heavier, laggier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation Lag", meta = (ClampMin = "0.5", ClampMax = "20.0"))
	float RotationLagFrequency = 4.5f;

	/** < 1 overshoots slightly and wobbles back, 1 = no overshoot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation Lag", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float RotationLagDamping = 0.6f;

	/** The camera is never allowed to trail the aim by more than this many degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation Lag", meta = (ClampMin = "1.0", ClampMax = "90.0"))
	float MaxRotationLag = 28.f;

	// ---- Springs for position / rotation offsets ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Springs", meta = (ClampMin = "0.5", ClampMax = "20.0"))
	float PositionFrequency = 7.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Springs", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float PositionDamping = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Springs", meta = (ClampMin = "0.5", ClampMax = "20.0"))
	float RotationFrequency = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Springs", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float RotationDamping = 0.4f;

	/** Camera roll offset is clamped to +/- this many degrees (comfort limit). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Springs", meta = (ClampMin = "0.0", ClampMax = "30.0"))
	float MaxRoll = 6.f;

	// ---- Reaction to acceleration (starting, stopping, strafing) ----

	/** cm of positional lag per 1000 cm/s^2 of acceleration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acceleration")
	float AccelerationOffset = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acceleration")
	float MaxAccelerationOffset = 4.f;

	/** Degrees of forward/back tilt per 1000 cm/s^2 (accelerating forward tips the camera down). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acceleration")
	float AccelerationPitch = 0.8f;

	/** Degrees of roll per 1000 cm/s^2 of sideways acceleration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acceleration")
	float AccelerationRoll = 0.8f;

	/** Acceleration is clamped to this magnitude (cm/s^2) to ignore teleports / huge spikes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acceleration")
	float MaxAcceleration = 4000.f;

	/** How fast the measured acceleration is smoothed (higher = snappier). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acceleration", meta = (ClampMin = "1.0"))
	float AccelerationSmoothing = 12.f;

	// ---- Jumping and landing ----

	/** Downward velocity kick (cm/s) per cm/s of fall speed at landing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump / Land")
	float LandingImpulse = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump / Land")
	float MaxLandingImpulse = 220.f;

	/** Pitch-down velocity kick (deg/s) per cm/s of fall speed at landing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump / Land")
	float LandingPitchImpulse = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump / Land")
	float MaxLandingPitchImpulse = 45.f;

	/** Small downward dip (cm/s) when leaving the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump / Land")
	float JumpImpulse = 90.f;

	// ---- Idle ----

	/** Breathing up/down motion in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Idle")
	float BreathAmplitude = 0.25f;

	/** Breaths per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Idle")
	float BreathRate = 0.28f;

	/** Slow random hand/body drift in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Idle")
	float HandheldAmplitude = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Idle")
	float HandheldFrequency = 0.9f;

	// ---- Locomotion ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	FBodycamMotionSettings Walk;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	FBodycamMotionSettings Sprint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	FBodycamMotionSettings Crouch;

	/** Ground speed (cm/s) above which the character counts as sprinting. Set between your walk and sprint speeds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	float SprintSpeedThreshold = 450.f;

	/** Ground speed (cm/s) below which the character counts as standing still. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	float MinMoveSpeed = 10.f;

	/** How fast bob amplitudes fade in/out when starting or stopping. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion", meta = (ClampMin = "0.5"))
	float MoveWeightRate = 8.f;

	/** How fast the walk/sprint/crouch settings blend into each other. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion", meta = (ClampMin = "0.5"))
	float MotionBlendRate = 6.f;

	/** Smooth the camera height when the capsule shrinks/grows (crouch). Requires the camera to be attached to the capsule. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion")
	bool bSmoothStanceChange = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion", meta = (EditCondition = "bSmoothStanceChange", ClampMin = "0.5"))
	float StanceFrequency = 3.5f;

	// ---- Lens / sensor look ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens")
	FBodycamLensSettings Lens;

	/**
	 * Post-process material (domain: Post Process, blendable location: After Tonemapping) that exposes the scalar
	 * parameters Distortion, RollingShutter, YawRate, Compression, SensorNoise, Aspect, ViewWidth, ViewHeight.
	 * See README.md for how to create it (or run Scripts/CreateBodycamMaterial.py).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens")
	TObjectPtr<UMaterialInterface> LensMaterial;
};
