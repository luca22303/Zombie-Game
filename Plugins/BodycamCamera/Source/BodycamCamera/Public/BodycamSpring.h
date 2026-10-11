#pragma once

#include "CoreMinimal.h"

/**
 * Small spring helpers used by the bodycam camera. Header-only, no UObject dependencies.
 *
 * Springs are integrated with fixed substeps (semi-implicit Euler) so the feel does not
 * change with frame rate. Frequencies are given in Hz, damping as a ratio
 * (1 = critically damped, < 1 = overshoots / bouncy, > 1 = sluggish).
 */
namespace BodycamMath
{
	constexpr double TwoPi = 6.283185307179586;
	constexpr double MaxSubstep = 1.0 / 240.0;
	constexpr double MaxFrequencyHz = 20.0;

	inline int32 NumSubsteps(double DeltaTime)
	{
		return FMath::Clamp(FMath::CeilToInt(DeltaTime / MaxSubstep), 1, 64);
	}

	/** Smooth pseudo-random value in roughly [-1, 1] (sum of incommensurate sines). */
	inline double SmoothNoise(double T, double Seed)
	{
		return (FMath::Sin(T + Seed)
			+ 0.5 * FMath::Sin(T * 2.17 + Seed * 1.7)
			+ 0.25 * FMath::Sin(T * 3.71 + Seed * 2.3)) / 1.75;
	}

	/** Shortest-arc rotation as an axis * angle (radians) vector. */
	inline FVector QuatToRotationVector(FQuat Q)
	{
		Q.Normalize();
		if (Q.W < 0.0)
		{
			Q = FQuat(-Q.X, -Q.Y, -Q.Z, -Q.W);
		}
		const FVector Xyz(Q.X, Q.Y, Q.Z);
		const double S = Xyz.Size();
		if (S < 1e-8)
		{
			return Xyz * 2.0;
		}
		return Xyz * (2.0 * FMath::Atan2(S, Q.W) / S);
	}

	inline FQuat RotationVectorToQuat(const FVector& V)
	{
		const double Angle = V.Size();
		if (Angle < 1e-8)
		{
			return FQuat(V.X * 0.5, V.Y * 0.5, V.Z * 0.5, 1.0).GetNormalized();
		}
		return FQuat(V / Angle, Angle);
	}
}

/** Mass-spring-damper on a vector. Position/Velocity are in whatever units the caller uses. */
struct FBodycamSpringVector
{
	FVector Position = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;

	void Reset()
	{
		Position = FVector::ZeroVector;
		Velocity = FVector::ZeroVector;
	}

	/** Instantly change velocity (kick). */
	void AddImpulse(const FVector& DeltaVelocity)
	{
		Velocity += DeltaVelocity;
	}

	void Step(const FVector& Target, double DeltaTime, double FrequencyHz, double DampingRatio)
	{
		const double Omega = BodycamMath::TwoPi * FMath::Clamp(FrequencyHz, 0.0, BodycamMath::MaxFrequencyHz);
		const double Omega2 = Omega * Omega;
		const double TwoZetaOmega = 2.0 * FMath::Max(DampingRatio, 0.0) * Omega;
		const int32 Steps = BodycamMath::NumSubsteps(DeltaTime);
		const double H = DeltaTime / Steps;

		for (int32 i = 0; i < Steps; ++i)
		{
			const FVector Accel = (Target - Position) * Omega2 - Velocity * TwoZetaOmega;
			Velocity += Accel * H;
			Position += Velocity * H;
		}
	}
};

/** Spring on an orientation: the rotation chases a target quaternion with overshoot. */
struct FBodycamSpringQuat
{
	FQuat Rotation = FQuat::Identity;
	/** World-space angular velocity in radians/second. */
	FVector AngularVelocity = FVector::ZeroVector;

	void Snap(const FQuat& NewRotation)
	{
		Rotation = NewRotation;
		AngularVelocity = FVector::ZeroVector;
	}

	void Step(const FQuat& Target, double DeltaTime, double FrequencyHz, double DampingRatio)
	{
		const double Omega = BodycamMath::TwoPi * FMath::Clamp(FrequencyHz, 0.0, BodycamMath::MaxFrequencyHz);
		const double Omega2 = Omega * Omega;
		const double TwoZetaOmega = 2.0 * FMath::Max(DampingRatio, 0.0) * Omega;
		const int32 Steps = BodycamMath::NumSubsteps(DeltaTime);
		const double H = DeltaTime / Steps;

		for (int32 i = 0; i < Steps; ++i)
		{
			// Rotation that takes the current orientation to the target.
			const FVector Error = BodycamMath::QuatToRotationVector(Target * Rotation.Inverse());
			const FVector Accel = Error * Omega2 - AngularVelocity * TwoZetaOmega;
			AngularVelocity += Accel * H;
			Rotation = BodycamMath::RotationVectorToQuat(AngularVelocity * H) * Rotation;
			Rotation.Normalize();
		}
	}
};
