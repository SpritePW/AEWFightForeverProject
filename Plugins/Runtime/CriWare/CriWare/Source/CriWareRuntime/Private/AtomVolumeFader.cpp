
#include "AtomVolumeFader.h"

namespace Atom
{
	// Low precision, high performance approximation of sine using parabolic polynomial approx
	// Valid on interval [-PI, PI]
	static FORCEINLINE float FastSin(const float X)
	{
		return (4.0f * X) / PI * (1.0f - FMath::Abs(X) / PI);
	}

	// Function to convert linear scale volume to decibels.
	static FORCEINLINE float ConvertToDecibels(const float InLinear, const float InFloor = SMALL_NUMBER)
	{
		return 20.0f * FMath::LogX(10.0f, FMath::Max(InLinear, InFloor));
	}

	// Function to convert decibel to linear scale.
	static FORCEINLINE float ConvertToLinear(const float InDecibels)
	{
		return FMath::Pow(10.0f, InDecibels / 20.0f);
	}
}

FAtomVolumeFader::FAtomVolumeFader()
	: Alpha(1.0f)
	, Target(1.0f)
	, ActiveDuration(-1.0f)
	, FadeDuration(-1.0f)
	, Elapsed(0.0f)
	, FadeCurve(EAtomFaderCurve::Linear)
{
}

float FAtomVolumeFader::AlphaToVolume(float InAlpha, EAtomFaderCurve InCurve)
{
	switch (InCurve)
	{
	case EAtomFaderCurve::Linear:
	{
		return InAlpha;
	}
	break;

	case EAtomFaderCurve::SCurve:
	{
		float Volume = 0.5f * Atom::FastSin(PI * InAlpha - HALF_PI) + 0.5f;
		return FMath::Max(0.0f, Volume);
	}
	break;

	case EAtomFaderCurve::Sin:
	{
		float Volume = Atom::FastSin(HALF_PI * InAlpha);
		return FMath::Max(0.0f, Volume);
	}
	break;

	case EAtomFaderCurve::Logarithmic:
	{
		return Atom::ConvertToLinear(InAlpha);
	}
	break;

	default:
	{
		static_assert(static_cast<int32>(EAtomFaderCurve::Count) == 4, "Possible missing switch case coverage for EAtomFaderCurve");
	}
	break;
	}

	return 1.0f;
}

void FAtomVolumeFader::Activate()
{
	if (ActiveDuration == 0.0f)
	{
		ActiveDuration = -1.0f;
	}
}

void FAtomVolumeFader::Deactivate()
{
	StopFade();
	ActiveDuration = 0.0f;
}

float FAtomVolumeFader::GetActiveDuration() const
{
	return ActiveDuration;
}

float FAtomVolumeFader::GetVolume() const
{
	return AlphaToVolume(Alpha, FadeCurve);
}

float FAtomVolumeFader::GetVolumeAfterTime(float InDeltaTime) const
{
	InDeltaTime = FMath::Max(0.0f, InDeltaTime);

	// Keep stepping towards our target until we hit our stop time & Clamp
	float FutureAlpha = Alpha;
	const float Duration = Elapsed + InDeltaTime;

	// If set to deactivate first, return default volume value of 1.0f
	if (ActiveDuration >= 0.0f && Duration > ActiveDuration)
	{
		return 1.0f;
	}

	if (Duration < FadeDuration)
	{
		// Choose min/max bound and clamp dt to prevent unwanted spikes in volume
		float MinValue = 0.0f;
		float MaxValue = 0.0f;
		if (Alpha < Target)
		{
			MinValue = Alpha;
			MaxValue = Target;
		}
		else
		{
			MinValue = Target;
			MaxValue = Alpha;
		}

		FutureAlpha = Alpha + ((Target - Alpha) * InDeltaTime / (FadeDuration - Elapsed));
		FutureAlpha = FMath::Clamp(FutureAlpha, MinValue, MaxValue);
	}

	return AlphaToVolume(FutureAlpha, FadeCurve);
}

float FAtomVolumeFader::GetFadeDuration() const
{
	return FadeDuration;
}

EAtomFaderCurve FAtomVolumeFader::GetCurve() const
{
	return FadeCurve;
}

float FAtomVolumeFader::GetTargetVolume() const
{
	switch (FadeCurve)
	{
	case EAtomFaderCurve::Linear:
	case EAtomFaderCurve::SCurve:
	case EAtomFaderCurve::Sin:
	{
		return Target;
	}
	break;

	case EAtomFaderCurve::Logarithmic:
	{
		return Atom::ConvertToLinear(Target);
	}
	break;

	default:
	{
		static_assert(static_cast<int32>(EAtomFaderCurve::Count) == 4, "Possible missing switch case coverage for EAtomFaderCurve");
	}
	break;
	}

	return 1.0f;
}

bool FAtomVolumeFader::IsActive() const
{
	return Elapsed < ActiveDuration || ActiveDuration < 0.0f;
}

bool FAtomVolumeFader::IsFading() const
{
	return Elapsed < FadeDuration;
}

bool FAtomVolumeFader::IsFadingIn() const
{
	return IsFading() && Target > Alpha;
}

bool FAtomVolumeFader::IsFadingOut() const
{
	return IsFading() && Target < Alpha;
}

void FAtomVolumeFader::SetActiveDuration(float InDuration)
{
	if (InDuration < 0.0f)
	{
		Activate();
	}
	else
	{
		ActiveDuration = InDuration;
		Elapsed = 0.0f;
	}
}

void FAtomVolumeFader::SetVolume(float InVolume)
{
	Alpha = InVolume;
	Elapsed = 0.0f;
	FadeCurve = EAtomFaderCurve::Linear;
	FadeDuration = -1.0f;
	Target = InVolume;
}

void FAtomVolumeFader::StartFade(float InVolume, float InDuration, EAtomFaderCurve InCurve)
{
	if (InDuration <= 0.0f)
	{
		SetVolume(InVolume);
		return;
	}

	if (InCurve != EAtomFaderCurve::Logarithmic)
	{
		if (FadeCurve == EAtomFaderCurve::Logarithmic)
		{
			Alpha = Atom::ConvertToLinear(Alpha);
		}
		Target = InVolume;
	}
	else
	{
		const float DecibelFloor = KINDA_SMALL_NUMBER; // -80dB
		if (FadeCurve != EAtomFaderCurve::Logarithmic)
		{
			Alpha = Atom::ConvertToDecibels(Alpha, DecibelFloor);
		}
		Target = Atom::ConvertToDecibels(InVolume, DecibelFloor);
	}

	Elapsed = 0.0f;
	FadeCurve = InCurve;
	FadeDuration = InDuration;
}

void FAtomVolumeFader::StopFade()
{
	if (FadeCurve == EAtomFaderCurve::Logarithmic)
	{
		Alpha = Atom::ConvertToLinear(Alpha);
	}
	Target = Alpha;
	FadeCurve = EAtomFaderCurve::Linear;
	Elapsed = ActiveDuration;
	FadeDuration = -1.0f;
}

void FAtomVolumeFader::Update(float InDeltaTime)
{
	// querying state before incrementing Elapsed time
	// lets sounds with a fade-in time < InDeltaTime play
	const bool bIsFading = IsFading();
	const bool bIsActive = IsActive();

	Elapsed += InDeltaTime;

	if (!bIsFading || !bIsActive)
	{
		return;
	}

	// Keep stepping towards target & clamp until fade duration has expired
	// Choose min/max bound and clamp dt to prevent unwanted spikes in volume
	float MinValue = 0.0f;
	float MaxValue = 0.0f;

	if (FadeDuration < Elapsed)
	{
		Alpha = Target;
		StopFade();
		return;
	}
	else if (Alpha < Target)
	{
		MinValue = Alpha;
		MaxValue = Target;
	}
	else 
	{
		MinValue = Target;
		MaxValue = Alpha;
	}

	Alpha += (Target - Alpha) * InDeltaTime / (FadeDuration - Elapsed);
	Alpha = FMath::Clamp(Alpha, MinValue, MaxValue);


	// Optimization that avoids fade calculations once alpha reaches target
	if (InDeltaTime > SMALL_NUMBER && FMath::IsNearlyEqual(Alpha, Target))
	{
		StopFade();
	}
}
