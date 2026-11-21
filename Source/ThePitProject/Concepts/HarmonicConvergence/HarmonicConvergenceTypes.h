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

UENUM(BlueprintType)
enum class EChordVoiceRole : uint8
{
	LowestVoice = 0 UMETA(DisplayName = "Lowest Chord Voice (Bass of Inversion)"),
	MiddleVoice = 1 UMETA(DisplayName = "Middle Chord Voice"),
	HighestVoice = 2 UMETA(DisplayName = "Highest Chord Voice (Top of Inversion)"),
	FixedPitch = 3 UMETA(DisplayName = "Fixed Scale Pitch (No Chord Tracking)")
};

USTRUCT(BlueprintType)
struct FHarmonicChord
{
	GENERATED_BODY()

	// ponytail: bounded close-position triad window [-5, +16] prevents infinite register drift; widen or allow open voicings if 7th/extended chords are added later
	static constexpr int32 MinVoiceSemitone = -5;
	static constexpr int32 MaxVoiceSemitone = 16;

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

	// Single source of truth for 7-note scale semitone intervals relative to root; returns false for CustomFrequencies
	static FORCEINLINE bool GetScaleIntervals(EHarmonicScaleMode ScaleMode, int32 OutIntervals[7])
	{
		static constexpr int32 ScaleTable[4][7] = {
			{ 0, 2, 4, 6, 7, 9, 11 }, // LydianCelestial
			{ 0, 2, 3, 5, 7, 9, 10 }, // DorianAmbient
			{ 0, 2, 4, 5, 7, 9, 11 }, // IonianMajor
			{ 0, 2, 3, 5, 7, 8, 10 }  // AeolianMinor
		};

		const bool bValid = (ScaleMode != EHarmonicScaleMode::CustomFrequencies);
		const int32 Idx = bValid ? FMath::Clamp(static_cast<int32>(ScaleMode), 0, 3) : 0;
		for (int32 i = 0; i < 7; ++i)
		{
			OutIntervals[i] = ScaleTable[Idx][i];
		}
		return bValid;
	}

	// Build a diatonic triad from a 0-based scale degree (0 = I .. 6 = VII) within the given 7-note scale
	static FORCEINLINE FHarmonicChord FromScaleDegree(EHarmonicScaleMode ScaleMode, int32 ScaleDegree, int32 InInversion = 0)
	{
		const int32 Deg = ((ScaleDegree % 7) + 7) % 7;
		int32 Intervals[7];
		GetScaleIntervals(ScaleMode, Intervals);

		const int32 Root = Intervals[Deg];
		const int32 ThirdSpan = Intervals[(Deg + 2) % 7] + (Deg + 2 >= 7 ? 12 : 0) - Root;
		const int32 FifthSpan = Intervals[(Deg + 4) % 7] + (Deg + 4 >= 7 ? 12 : 0) - Root;

		const EHarmonicChordQuality DiatonicQuality =
			(ThirdSpan == 3 && FifthSpan == 6) ? EHarmonicChordQuality::Diminished :
			(ThirdSpan == 3)                   ? EHarmonicChordQuality::Minor :
			(FifthSpan == 8)                   ? EHarmonicChordQuality::Augmented :
			                                     EHarmonicChordQuality::Major;

		return FHarmonicChord{ Root, DiatonicQuality, FMath::Clamp(InInversion, 0, 2) };
	}

	FORCEINLINE void GetQualityIntervals(int32& OutThird, int32& OutFifth) const
	{
		static constexpr int32 Intervals[4][2] = { { 4, 7 }, { 3, 7 }, { 3, 6 }, { 4, 8 } };
		const int32 Q = FMath::Clamp(static_cast<int32>(Quality), 0, 3);
		OutThird = Intervals[Q][0];
		OutFifth = Intervals[Q][1];
	}

	FORCEINLINE void BuildCloseShape(int32 BaseRootSemis, int32 InInversion, int32 OutVoices[3]) const
	{
		int32 Third = 4, Fifth = 7;
		GetQualityIntervals(Third, Fifth);
		const int32 Triad[3] = { 0, Third, Fifth };
		const int32 Inv = FMath::Clamp(InInversion, 0, 2);
		for (int32 i = 0; i < 3; ++i)
		{
			OutVoices[i] = BaseRootSemis + Triad[(Inv + i) % 3] + (Inv + i >= 3 ? 12 : 0);
		}
	}

	FORCEINLINE void GetCloseVoicing(int32 OutVoices[3], int32 InInversion) const
	{
		BuildCloseShape(RootSemitoneOffset, InInversion, OutVoices);

		while (OutVoices[0] < MinVoiceSemitone)
		{
			for (int32 i = 0; i < 3; ++i) { OutVoices[i] += 12; }
		}
		while (OutVoices[2] > MaxVoiceSemitone)
		{
			for (int32 i = 0; i < 3; ++i) { OutVoices[i] -= 12; }
		}
	}

	FORCEINLINE void ChooseBestVoiceLeading(const int32 PrevVoices[3], int32 OutVoices[3], int32& OutChosenInversion) const
	{
		const int32 RootPC = ((RootSemitoneOffset % 12) + 12) % 12;

		int32 BestTotalDist = MAX_int32;
		int32 BestMaxLeap = MAX_int32;
		int32 BestCenterDist = MAX_int32;
		int32 BestInv = 0;
		int32 BestVoices[3] = { 0, 0, 0 };

		for (int32 Inv = 0; Inv <= 2; ++Inv)
		{
			int32 BaseShape[3];
			BuildCloseShape(RootPC, Inv, BaseShape);

			for (int32 Oct = -2; Oct <= 1; ++Oct)
			{
				const int32 Shift = Oct * 12;
				const int32 Cand[3] = { BaseShape[0] + Shift, BaseShape[1] + Shift, BaseShape[2] + Shift };
				if (Cand[0] < MinVoiceSemitone || Cand[2] > MaxVoiceSemitone)
				{
					continue;
				}

				const int32 D0 = FMath::Abs(Cand[0] - PrevVoices[0]);
				const int32 D1 = FMath::Abs(Cand[1] - PrevVoices[1]);
				const int32 D2 = FMath::Abs(Cand[2] - PrevVoices[2]);
				const int32 TotalDist = D0 + D1 + D2;
				const int32 MaxLeap = FMath::Max3(D0, D1, D2);
				const int32 CenterDist = FMath::Abs((Cand[0] + Cand[1] + Cand[2]) - 12);

				const bool bBetter =
					(TotalDist < BestTotalDist) ||
					(TotalDist == BestTotalDist && MaxLeap < BestMaxLeap) ||
					(TotalDist == BestTotalDist && MaxLeap == BestMaxLeap && CenterDist < BestCenterDist) ||
					(TotalDist == BestTotalDist && MaxLeap == BestMaxLeap && CenterDist == BestCenterDist && Inv < BestInv);

				if (bBetter)
				{
					BestTotalDist = TotalDist;
					BestMaxLeap = MaxLeap;
					BestCenterDist = CenterDist;
					BestInv = Inv;
					BestVoices[0] = Cand[0];
					BestVoices[1] = Cand[1];
					BestVoices[2] = Cand[2];
				}
			}
		}

		if (BestTotalDist == MAX_int32)
		{
			GetCloseVoicing(OutVoices, Inversion);
			OutChosenInversion = FMath::Clamp(Inversion, 0, 2);
			return;
		}

		for (int32 i = 0; i < 3; ++i)
		{
			OutVoices[i] = BestVoices[i];
		}
		OutChosenInversion = BestInv;
	}
};

// One row of the 7x7 Markov chord transition table (relative transition weights from FromDegree to I..VII)
USTRUCT(BlueprintType)
struct FMarkovChordRow
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Markov")
	FName FromDegree = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Harmonics|Markov", meta = (ClampMin = "0.0"))
	float Weights[7] = { 50.0f, 50.0f, 33.0f, 66.0f, 66.0f, 33.0f, 10.0f };

	FORCEINLINE float GetWeight(int32 Degree) const
	{
		return (Degree >= 0 && Degree < 7) ? Weights[Degree] : 0.0f;
	}

	FORCEINLINE int32 SampleNextDegree(float NormalizedRand01, int32 FallbackDegree) const
	{
		float Total = 0.0f;
		int32 LastPositiveIdx = -1;
		for (int32 i = 0; i < 7; ++i)
		{
			const float W = FMath::Max(0.0f, Weights[i]);
			if (W > 0.0f)
			{
				Total += W;
				LastPositiveIdx = i;
			}
		}

		if (Total <= KINDA_SMALL_NUMBER || LastPositiveIdx < 0)
		{
			return ((FallbackDegree % 7) + 7) % 7;
		}

		const float Target = FMath::Clamp(NormalizedRand01, 0.0f, 0.999999f) * Total;
		float Acc = 0.0f;
		for (int32 i = 0; i < 7; ++i)
		{
			const float W = FMath::Max(0.0f, Weights[i]);
			if (W > 0.0f && (Acc += W) > Target)
			{
				return i;
			}
		}

		return LastPositiveIdx;
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

// One row of the 4x4 Markov Scale Mode transition table (relative weights to LydianCelestial, DorianAmbient, IonianMajor, AeolianMinor)
USTRUCT(BlueprintType)
struct FMarkovScaleRow
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Markov")
	FName FromMode = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Harmonics|Markov", meta = (ClampMin = "0.0"))
	float Weights[4] = { 35.0f, 35.0f, 15.0f, 15.0f };

	FORCEINLINE EHarmonicScaleMode SampleNextMode(const float DynamicBias[4], float NormalizedRand01, EHarmonicScaleMode FallbackMode) const
	{
		float Effective[4];
		float Total = 0.0f;
		int32 LastPositiveIdx = -1;
		for (int32 i = 0; i < 4; ++i)
		{
			Effective[i] = FMath::Max(0.0f, Weights[i] * FMath::Max(0.0f, DynamicBias[i]));
			if (Effective[i] > 0.0f)
			{
				Total += Effective[i];
				LastPositiveIdx = i;
			}
		}

		if (Total <= KINDA_SMALL_NUMBER || LastPositiveIdx < 0)
		{
			return FallbackMode;
		}

		const float Target = FMath::Clamp(NormalizedRand01, 0.0f, 0.999999f) * Total;
		float Acc = 0.0f;
		for (int32 i = 0; i < 4; ++i)
		{
			if (Effective[i] > 0.0f && (Acc += Effective[i]) > Target)
			{
				return static_cast<EHarmonicScaleMode>(i);
			}
		}

		return static_cast<EHarmonicScaleMode>(LastPositiveIdx);
	}
};
