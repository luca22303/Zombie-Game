#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BodycamHandGripComponent.generated.h"

class USkeletalMeshComponent;
class USceneComponent;

/** One arm that is pinned to a grip point on the weapon. */
USTRUCT(BlueprintType)
struct FBodycamHandGrip
{
	GENERATED_BODY()

	/** Pin this hand to the weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	bool bEnabled = true;

	/** Where the hand sits, in WEAPON component space (cm, AKM: origin at the top of the pistol grip, barrel along +X). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	FTransform GripInWeaponSpace = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	FName UpperArmBone;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	FName LowerArmBone;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	FName HandBone;

	/** 0 keeps the animated hand orientation (only the position is pinned), 1 rotates the hand to the grip transform. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RotationWeight = 0.f;
};

/**
 * Keeps the viewmodel hands on the weapon.
 *
 * The animated arms never know where the weapon really is once stances, the weapon pivot (collision, sprint lock) or a
 * different weapon move it relative to the hands. This component runs after the arm animation has been evaluated and
 * solves a two-bone IK (upper arm, lower arm, hand) so each enabled hand reaches its grip point on the weapon. The bend
 * direction of the elbow is taken from the animation, so no pole target needs tuning. Bones below the arm (fingers,
 * twist bones) are carried along rigidly.
 *
 * Left hand: pinned by default (support hand under the handguard).
 * Right hand: off by default. The weapon is attached to the right hand bone, so the weapon already follows it; enable the
 * right hand only if the weapon is attached to something that is NOT the right hand chain (otherwise the hand would chase a
 * weapon that is chasing the hand).
 *
 * It hooks USkeletalMeshComponent::OnBoneTransformsFinalized, so no Animation Blueprint node is needed. If your engine
 * version does not let you edit the bones there, read GetGripWorldTransform() and feed it into a Two Bone IK node in
 * ABP_Rifle_FP instead (README section 8).
 */
UCLASS(ClassGroup = (Camera), meta = (BlueprintSpawnableComponent))
class BODYCAMCAMERA_API UBodycamHandGripComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBodycamHandGripComponent();

	/** Name of the skeletal mesh that carries the viewmodel arms. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	FName ArmsComponentName = TEXT("ViewmodelArms");

	/** Name of the weapon component (a child of the arms mesh, attached to a hand socket or bone). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	FName WeaponComponentName = TEXT("Weapon");

	/** Overall strength, 0 = pure animation, 1 = fully pinned. Blend it to 0 while reloading or playing a hand animation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Alpha = 1.f;

	/**
	 * Support hand. Default bone names are the UE5 mannequin's. GripInWeaponSpace is a first guess under the AKM
	 * handguard: turn on DebugDraw and move it until the palm sits right.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	FBodycamHandGrip LeftHand;

	/** Trigger hand, off by default (see class comment). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip")
	FBodycamHandGrip RightHand;

	/** Draw both grip points (blue = left, red = right) and a line to the animated hand. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDebugDraw = false;

	UFUNCTION(BlueprintCallable, Category = "Grip")
	void SetMeshes(USkeletalMeshComponent* NewArms, USceneComponent* NewWeapon);

	/** World transform of a grip point on the weapon, for driving a Two Bone IK node in an Animation Blueprint instead. */
	UFUNCTION(BlueprintPure, Category = "Grip")
	FTransform GetGripWorldTransform(bool bLeftHand) const;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	struct FChain;

	void Bind();
	void Unbind();
	void OnBonesFinalized();
	void SolveHand(const FBodycamHandGrip& Grip, FChain& Chain, TArray<FTransform>& ComponentSpace, const FTransform& WeaponInArms);
	void BuildChain(const FBodycamHandGrip& Grip, FChain& OutChain) const;
	bool ComputeWeaponInArms(const TArray<FTransform>& ComponentSpace, FTransform& OutWeaponInArms) const;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> Arms;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Weapon;

	/** Bone indices and per-bone driver slot (which of the three IK bones a bone follows), built once per skeleton. */
	struct FChain
	{
		int32 Bones[3] = { INDEX_NONE, INDEX_NONE, INDEX_NONE };
		TArray<int8> Driver;
		bool bValid = false;
	};

	FChain LeftChain;
	FChain RightChain;
	FDelegateHandle BoneHandle;
	bool bWarned = false;
};
