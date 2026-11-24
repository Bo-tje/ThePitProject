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

UENUM(BlueprintType)
enum class EVoiceTimbreProfile : uint8
{
	WarmPad UMETA(DisplayName = "Warm Analog Pad (Dual-Detuned Sustained)"),
	SubBassPad UMETA(DisplayName = "Deep Sub-Bass Anchor"),
	CrystallineChime UMETA(DisplayName = "Crystalline Chime / Glass Marimba Pluck"),
	VortexSweep UMETA(DisplayName = "Resonant Vortex Drone")
};

FORCEINLINE int32 SampleWeightedIndex(const float* Weights, int32 Count, float NormalizedRand01, int32 FallbackIndex)
{
	float TotalWeight = 0.0f;
	int32 LastPositiveIndex = -1;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float PositiveWeight = FMath::Max(0.0f, Weights[Index]);
		if (PositiveWeight > 0.0f)
		{
			TotalWeight += PositiveWeight;
			LastPositiveIndex = Index;
		}
	}

	if (TotalWeight <= KINDA_SMALL_NUMBER || LastPositiveIndex < 0)
	{
		return ((FallbackIndex % Count) + Count) % Count;
	}

	const float RouletteTarget = FMath::Clamp(NormalizedRand01, 0.0f, 0.999999f) * TotalWeight;
	float AccumulatedWeight = 0.0f;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float PositiveWeight = FMath::Max(0.0f, Weights[Index]);
		if (PositiveWeight > 0.0f && (AccumulatedWeight += PositiveWeight) > RouletteTarget)
		{
			return Index;
		}
	}

	return LastPositiveIndex;
}

USTRUCT(BlueprintType)
struct FHarmonicChord
{
	GENERATED_BODY()

	// ponytail: bounded close-position triad window [-5, +16] prevents infinite register drift; widen or allow open voicings if 7th/extended chords are added later
	static constexpr int32 MinVoiceSemitone = -5;
	static constexpr int32 MaxVoiceSemitone = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Harmony", meta = (ClampMin = "-12", ClampMax = "12"))
	int32 RootSemitoneOffset = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Harmony")
	EHarmonicChordQuality Quality = EHarmonicChordQuality::Minor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Harmony", meta = (ClampMin = "0", ClampMax = "2"))
	int32 Inversion = 0;

	static FORCEINLINE float SemitonesToRatio(float Semitones)
	{
		return FMath::Pow(2.0f, Semitones / 12.0f);
	}

	static FORCEINLINE bool GetScaleIntervals(EHarmonicScaleMode ScaleMode, int32 OutIntervals[7])
	{
		static constexpr int32 ScaleSemitoneTable[4][7] = {
			{ 0, 2, 4, 6, 7, 9, 11 }, // LydianCelestial
			{ 0, 2, 3, 5, 7, 9, 10 }, // DorianAmbient
			{ 0, 2, 4, 5, 7, 9, 11 }, // IonianMajor
			{ 0, 2, 3, 5, 7, 8, 10 }  // AeolianMinor
		};

		const bool bIsDiatonicScale = (ScaleMode != EHarmonicScaleMode::CustomFrequencies);
		const int32 ModeIndex = bIsDiatonicScale ? FMath::Clamp(static_cast<int32>(ScaleMode), 0, 3) : 0;
		for (int32 NoteIndex = 0; NoteIndex < 7; ++NoteIndex)
		{
			OutIntervals[NoteIndex] = ScaleSemitoneTable[ModeIndex][NoteIndex];
		}
		return bIsDiatonicScale;
	}

	static FORCEINLINE FHarmonicChord FromScaleDegree(EHarmonicScaleMode ScaleMode, int32 ScaleDegree, int32 InInversion = 0)
	{
		const int32 WrappedDegree = ((ScaleDegree % 7) + 7) % 7;
		int32 ScaleIntervals[7];
		GetScaleIntervals(ScaleMode, ScaleIntervals);

		const int32 RootSemitone = ScaleIntervals[WrappedDegree];
		const int32 ThirdIntervalSemitones = ScaleIntervals[(WrappedDegree + 2) % 7] + (WrappedDegree + 2 >= 7 ? 12 : 0) - RootSemitone;
		const int32 FifthIntervalSemitones = ScaleIntervals[(WrappedDegree + 4) % 7] + (WrappedDegree + 4 >= 7 ? 12 : 0) - RootSemitone;

		const EHarmonicChordQuality DiatonicQuality =
			(ThirdIntervalSemitones == 3 && FifthIntervalSemitones == 6) ? EHarmonicChordQuality::Diminished :
			(ThirdIntervalSemitones == 3)                                 ? EHarmonicChordQuality::Minor :
			(FifthIntervalSemitones == 8)                                 ? EHarmonicChordQuality::Augmented :
			                                                                EHarmonicChordQuality::Major;

		return FHarmonicChord{ RootSemitone, DiatonicQuality, FMath::Clamp(InInversion, 0, 2) };
	}

	FORCEINLINE void GetQualityIntervals(int32& OutThirdSemitones, int32& OutFifthSemitones) const
	{
		static constexpr int32 QualitySemitoneTable[4][2] = { { 4, 7 }, { 3, 7 }, { 3, 6 }, { 4, 8 } };
		const int32 QualityIndex = FMath::Clamp(static_cast<int32>(Quality), 0, 3);
		OutThirdSemitones = QualitySemitoneTable[QualityIndex][0];
		OutFifthSemitones = QualitySemitoneTable[QualityIndex][1];
	}

	FORCEINLINE void BuildCloseShape(int32 BaseRootSemitones, int32 InInversion, int32 OutVoices[3]) const
	{
		int32 ThirdSemitones = 4, FifthSemitones = 7;
		GetQualityIntervals(ThirdSemitones, FifthSemitones);
		const int32 TriadIntervals[3] = { 0, ThirdSemitones, FifthSemitones };
		const int32 ClampedInversion = FMath::Clamp(InInversion, 0, 2);
		for (int32 VoiceSlot = 0; VoiceSlot < 3; ++VoiceSlot)
		{
			const int32 TriadNoteIndex = ClampedInversion + VoiceSlot;
			OutVoices[VoiceSlot] = BaseRootSemitones + TriadIntervals[TriadNoteIndex % 3] + (TriadNoteIndex >= 3 ? 12 : 0);
		}
	}

	FORCEINLINE void GetCloseVoicing(int32 OutVoices[3], int32 InInversion) const
	{
		BuildCloseShape(RootSemitoneOffset, InInversion, OutVoices);

		while (OutVoices[0] < MinVoiceSemitone)
		{
			for (int32 VoiceSlot = 0; VoiceSlot < 3; ++VoiceSlot) { OutVoices[VoiceSlot] += 12; }
		}
		while (OutVoices[2] > MaxVoiceSemitone)
		{
			for (int32 VoiceSlot = 0; VoiceSlot < 3; ++VoiceSlot) { OutVoices[VoiceSlot] -= 12; }
		}
	}

	FORCEINLINE void ChooseBestVoiceLeading(const int32 PreviousVoices[3], int32 OutVoices[3], int32& OutChosenInversion) const
	{
		const int32 RootPitchClass = ((RootSemitoneOffset % 12) + 12) % 12;

		int32 BestTotalDistance = MAX_int32;
		int32 BestMaxVoiceLeap = MAX_int32;
		int32 BestRegisterCenterDistance = MAX_int32;
		int32 BestInversion = 0;
		int32 BestVoicing[3] = { 0, 0, 0 };

		for (int32 CandidateInversion = 0; CandidateInversion <= 2; ++CandidateInversion)
		{
			int32 UnshiftedTriad[3];
			BuildCloseShape(RootPitchClass, CandidateInversion, UnshiftedTriad);

			for (int32 OctaveShift = -2; OctaveShift <= 1; ++OctaveShift)
			{
				const int32 SemitoneShift = OctaveShift * 12;
				const int32 CandidateVoicing[3] = {
					UnshiftedTriad[0] + SemitoneShift,
					UnshiftedTriad[1] + SemitoneShift,
					UnshiftedTriad[2] + SemitoneShift
				};
				if (CandidateVoicing[0] < MinVoiceSemitone || CandidateVoicing[2] > MaxVoiceSemitone)
				{
					continue;
				}

				const int32 LowVoiceStep = FMath::Abs(CandidateVoicing[0] - PreviousVoices[0]);
				const int32 MidVoiceStep = FMath::Abs(CandidateVoicing[1] - PreviousVoices[1]);
				const int32 HighVoiceStep = FMath::Abs(CandidateVoicing[2] - PreviousVoices[2]);
				const int32 TotalVoiceDistance = LowVoiceStep + MidVoiceStep + HighVoiceStep;
				const int32 MaxVoiceLeap = FMath::Max3(LowVoiceStep, MidVoiceStep, HighVoiceStep);
				const int32 RegisterCenterDistance = FMath::Abs((CandidateVoicing[0] + CandidateVoicing[1] + CandidateVoicing[2]) - 12);

				const bool bIsSmootherTransition =
					(TotalVoiceDistance < BestTotalDistance) ||
					(TotalVoiceDistance == BestTotalDistance && MaxVoiceLeap < BestMaxVoiceLeap) ||
					(TotalVoiceDistance == BestTotalDistance && MaxVoiceLeap == BestMaxVoiceLeap && RegisterCenterDistance < BestRegisterCenterDistance) ||
					(TotalVoiceDistance == BestTotalDistance && MaxVoiceLeap == BestMaxVoiceLeap && RegisterCenterDistance == BestRegisterCenterDistance && CandidateInversion < BestInversion);

				if (bIsSmootherTransition)
				{
					BestTotalDistance = TotalVoiceDistance;
					BestMaxVoiceLeap = MaxVoiceLeap;
					BestRegisterCenterDistance = RegisterCenterDistance;
					BestInversion = CandidateInversion;
					BestVoicing[0] = CandidateVoicing[0];
					BestVoicing[1] = CandidateVoicing[1];
					BestVoicing[2] = CandidateVoicing[2];
				}
			}
		}

		if (BestTotalDistance == MAX_int32)
		{
			GetCloseVoicing(OutVoices, Inversion);
			OutChosenInversion = FMath::Clamp(Inversion, 0, 2);
			return;
		}

		for (int32 VoiceSlot = 0; VoiceSlot < 3; ++VoiceSlot)
		{
			OutVoices[VoiceSlot] = BestVoicing[VoiceSlot];
		}
		OutChosenInversion = BestInversion;
	}
};

USTRUCT(BlueprintType)
struct FMarkovChordRow
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Markov")
	FName FromDegree = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Harmonics|Markov", meta = (ClampMin = "0.0"))
	float Weights[7] = { 50.0f, 50.0f, 33.0f, 66.0f, 66.0f, 33.0f, 10.0f };

	FORCEINLINE int32 SampleNextDegree(const float DynamicBias[7], float NormalizedRand01, int32 FallbackDegree) const
	{
		float BiasedWeights[7];
		for (int32 DegreeIndex = 0; DegreeIndex < 7; ++DegreeIndex)
		{
			BiasedWeights[DegreeIndex] = FMath::Max(0.0f, Weights[DegreeIndex] * FMath::Max(0.0f, DynamicBias[DegreeIndex]));
		}
		return SampleWeightedIndex(BiasedWeights, 7, NormalizedRand01, FallbackDegree);
	}
};

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
		float BiasedWeights[4];
		for (int32 ModeIndex = 0; ModeIndex < 4; ++ModeIndex)
		{
			BiasedWeights[ModeIndex] = FMath::Max(0.0f, Weights[ModeIndex] * FMath::Max(0.0f, DynamicBias[ModeIndex]));
		}
		const int32 ChosenModeIndex = SampleWeightedIndex(BiasedWeights, 4, NormalizedRand01, static_cast<int32>(FallbackMode));
		return static_cast<EHarmonicScaleMode>(ChosenModeIndex);
	}
};
