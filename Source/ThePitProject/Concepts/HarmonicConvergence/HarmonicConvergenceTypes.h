#pragma once

#include "CoreMinimal.h"
#include "HarmonicConvergenceTypes.generated.h"

UENUM(BlueprintType)
enum class EHarmonicScaleMode : uint8
{
	LydianCelestial UMETA(DisplayName = "Lydian Celestial (7-Note: C D E F# G A B)"),
	DorianAmbient UMETA(DisplayName = "Dorian Ambient (7-Note: C D Eb F G A Bb)"),
	IonianMajor UMETA(DisplayName = "Ionian Major (7-Note: C D E F G A B)"),
	AeolianMinor UMETA(DisplayName = "Aeolian Natural Minor (7-Note: C D Eb F G Ab Bb)"),
	CustomFrequencies UMETA(DisplayName = "Custom Frequencies")
};

UENUM(BlueprintType)
enum class EHarmonicChordQuality : uint8
{
	Major UMETA(DisplayName = "Major (0, 4, 7)"),
	Minor UMETA(DisplayName = "Minor (0, 3, 7)"),
	Diminished UMETA(DisplayName = "Diminished (0, 3, 6)"),
	Augmented UMETA(DisplayName = "Augmented (0, 4, 8)")
};

USTRUCT(BlueprintType)
struct FHarmonicChord
{
	GENERATED_BODY()

	// Semitone offset relative to the scale root (e.g. 0 = I, -4 = bVI, 3 = bIII, -2 = bVII)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Harmony", meta = (ClampMin = "-12", ClampMax = "12"))
	int32 RootSemitoneOffset = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Harmony")
	EHarmonicChordQuality Quality = EHarmonicChordQuality::Minor;

	// 0 = Root position, 1 = 1st inversion, 2 = 2nd inversion
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Harmony", meta = (ClampMin = "0", ClampMax = "2"))
	int32 Inversion = 0;

	static FORCEINLINE float SemitonesToRatio(float Semitones)
	{
		return FMath::Pow(2.0f, Semitones / 12.0f);
	}

	FORCEINLINE void GetIntervalRatios(float& OutThirdRatio, float& OutFifthRatio) const
	{
		int32 ThirdSemis = 4;
		int32 FifthSemis = 7;
		switch (Quality)
		{
		case EHarmonicChordQuality::Minor:      ThirdSemis = 3; FifthSemis = 7; break;
		case EHarmonicChordQuality::Diminished: ThirdSemis = 3; FifthSemis = 6; break;
		case EHarmonicChordQuality::Augmented:  ThirdSemis = 4; FifthSemis = 8; break;
		case EHarmonicChordQuality::Major:
		default:                                ThirdSemis = 4; FifthSemis = 7; break;
		}
		if (Inversion >= 1) { /* root moves up an octave in pad voicing via fifth/third ratios */ ThirdSemis -= (Inversion == 2 ? 12 : 0); }
		if (Inversion == 2) { FifthSemis -= 12; }
		OutThirdRatio = SemitonesToRatio(static_cast<float>(ThirdSemis));
		OutFifthRatio = SemitonesToRatio(static_cast<float>(FifthSemis));
	}
};

UENUM(BlueprintType)
enum class EVoiceTimbreProfile : uint8
{
	WarmPad UMETA(DisplayName = "Warm Analog Pad (Dual-Detuned Sustained)"),
	SubBassPad UMETA(DisplayName = "Deep Sub-Bass Anchor"),
	CrystallineChime UMETA(DisplayName = "Crystalline Chime / Glass Marimba Pluck"),
	VortexSweep UMETA(DisplayName = "Resonant Vortex Drone")
};
