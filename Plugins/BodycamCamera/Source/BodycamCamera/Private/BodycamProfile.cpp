#include "BodycamProfile.h"

FBodycamMotionSettings FBodycamMotionSettings::Lerp(const FBodycamMotionSettings& A, const FBodycamMotionSettings& B, float Alpha)
{
	FBodycamMotionSettings R;
	R.ReferenceSpeed = FMath::Lerp(A.ReferenceSpeed, B.ReferenceSpeed, Alpha);
	R.StrideLength = FMath::Lerp(A.StrideLength, B.StrideLength, Alpha);
	R.VerticalBob = FMath::Lerp(A.VerticalBob, B.VerticalBob, Alpha);
	R.LateralBob = FMath::Lerp(A.LateralBob, B.LateralBob, Alpha);
	R.ForwardBob = FMath::Lerp(A.ForwardBob, B.ForwardBob, Alpha);
	R.RollBob = FMath::Lerp(A.RollBob, B.RollBob, Alpha);
	R.PitchBob = FMath::Lerp(A.PitchBob, B.PitchBob, Alpha);
	R.YawBob = FMath::Lerp(A.YawBob, B.YawBob, Alpha);
	R.FootstepImpulse = FMath::Lerp(A.FootstepImpulse, B.FootstepImpulse, Alpha);
	R.FootstepRotationImpulse = FMath::Lerp(A.FootstepRotationImpulse, B.FootstepRotationImpulse, Alpha);
	return R;
}

UBodycamProfile::UBodycamProfile()
{
	// Walk keeps the struct defaults.

	Sprint.ReferenceSpeed = 550.f;
	Sprint.StrideLength = 260.f;
	Sprint.VerticalBob = 3.0f;
	Sprint.LateralBob = 2.0f;
	Sprint.ForwardBob = 1.2f;
	Sprint.RollBob = 1.8f;
	Sprint.PitchBob = 1.5f;
	Sprint.YawBob = 1.0f;
	Sprint.FootstepImpulse = 70.f;
	Sprint.FootstepRotationImpulse = 16.f;

	Crouch.ReferenceSpeed = 150.f;
	Crouch.StrideLength = 110.f;
	Crouch.VerticalBob = 0.6f;
	Crouch.LateralBob = 0.5f;
	Crouch.ForwardBob = 0.2f;
	Crouch.RollBob = 0.4f;
	Crouch.PitchBob = 0.3f;
	Crouch.YawBob = 0.2f;
	Crouch.FootstepImpulse = 10.f;
	Crouch.FootstepRotationImpulse = 3.f;
}
