#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "HarmonicConvergenceTypes.h"
#include "HarmonicConvergenceSynthComponent.generated.h"

// Unconditionally Stable 2-Pole State-Variable Low-Pass Filter (Bilinear / Trapezoidal SVF)
struct FConvergenceResonantFilter
{
	float IntegratorState1 = 0.0f;
	float IntegratorState2 = 0.0f;

	inline float Process(float InputSample, float CutoffFrequencyHz, float ResonanceQualityQ, float SampleRate)
	{
		const float ClampedCutoffHz = FMath::Clamp(CutoffFrequencyHz, 20.0f, SampleRate * 0.45f);
		const float SafeQualityQ = FMath::Clamp(ResonanceQualityQ, 0.5f, 6.0f);

		const float TangentPrewarp = FMath::Tan(PI * (ClampedCutoffHz / SampleRate));
		const float DampingFactorK = 1.0f / SafeQualityQ;
		const float CoefficientA1 = 1.0f / (1.0f + TangentPrewarp * (TangentPrewarp + DampingFactorK));
		const float CoefficientA2 = TangentPrewarp * CoefficientA1;
		const float CoefficientA3 = TangentPrewarp * CoefficientA2;

		const float HighPassNode = InputSample - IntegratorState2;
		const float BandPassNode = CoefficientA1 * IntegratorState1 + CoefficientA2 * HighPassNode;
		const float LowPassNode = IntegratorState2 + CoefficientA2 * IntegratorState1 + CoefficientA3 * HighPassNode;

		IntegratorState1 = 2.0f * BandPassNode - IntegratorState1;
		IntegratorState2 = 2.0f * LowPassNode - IntegratorState2;

		if (!FMath::IsFinite(LowPassNode))
		{
			Reset();
			return 0.0f;
		}

		return LowPassNode;
	}

	inline void Reset()
	{
		IntegratorState1 = 0.0f;
		IntegratorState2 = 0.0f;
	}
};

// 4-Tap Stereo Diffusion Reverb Tank for cinematic ambient blending
struct FAmbientDiffusionTank
{
	static constexpr int32 MaxTapSize = 2243;
	static constexpr int32 TapSizes[4] = { 1447, 1693, 1979, 2243 };

	float DelayBuffers[4][MaxTapSize] = {};
	int32 WriteIndices[4] = {};
	float LowPassDamping[4] = {};

	inline void Process(float InLeftSample, float InRightSample, float FeedbackGain, float& OutLeftSample, float& OutRightSample)
	{
		const float ClampedFeedbackGain = FMath::Clamp(FeedbackGain, 0.0f, 0.88f);
		const float MonoInputSample = (InLeftSample + InRightSample) * 0.35f;
		float Delayed[4];

		for (int32 i = 0; i < 4; ++i)
		{
			Delayed[i] = DelayBuffers[i][WriteIndices[i]];
			LowPassDamping[i] = LowPassDamping[i] * 0.45f + Delayed[i] * 0.55f;
			DelayBuffers[i][WriteIndices[i]] = MonoInputSample + LowPassDamping[i] * ClampedFeedbackGain;
			if (++WriteIndices[i] >= TapSizes[i]) WriteIndices[i] = 0;
		}

		OutLeftSample = (Delayed[0] - Delayed[2]) * 0.5f;
		OutRightSample = (Delayed[1] - Delayed[3]) * 0.5f;
	}

	inline void Reset()
	{
		FMemory::Memzero(DelayBuffers, sizeof(DelayBuffers));
		FMemory::Memzero(WriteIndices, sizeof(WriteIndices));
		FMemory::Memzero(LowPassDamping, sizeof(LowPassDamping));
	}
};

// Particle Stream Voice Engine: Energy Capacitor Model (Tactile Pad Bite + Rising Charge Sweep)
struct FConvergenceVoiceDSP
{
	float NoteFrequencyHz = 261.63f;
	float CarrierPhaseAngle = 0.0f;
	float HarmonicPhaseAngle = 0.0f;
	float FlutterPhaseAngle = 0.0f;
	float HoldModulationIntensity = 0.0f;
	float StereoPanPosition = 0.0f;

	bool bIsVoiceActive = false;
	float AmplitudeEnvelope = 0.0f;

	// Tactile Note-On Strike Transient (Crisp ~70ms drum-pad attack bite)
	float TransientEnvelope = 0.0f;
	float TransientPhaseAngle = 0.0f;

	// Crystal Ignition Transient (Glass chime strike fired at 100% charge)
	float IgnitionEnvelope = 0.0f;
	float IgnitionPhaseAngle = 0.0f;

	float GenerateSample(float SampleRate, float ChordMultiplier, float& OutLeftSample, float& OutRightSample);

	void NoteOn()
	{
		bIsVoiceActive = true;
		TransientEnvelope = 1.0f;
		TransientPhaseAngle = 0.0f;
	}

	void NoteOff()
	{
		bIsVoiceActive = false;
	}

	void TriggerIgnition()
	{
		IgnitionEnvelope = 1.0f;
		IgnitionPhaseAngle = 0.0f;
	}
};

// Persistent Rhythmic Beat DSP Voice (Slot-based 4/4 quantized pulse)
struct FRhythmicPulseDSP
{
	bool bIsActive = false;
	int32 RhythmicPatternMask = 0x1111;
	float BaseFrequencyHz = 130.81f;
	EVoiceTimbreProfile TimbreProfile = EVoiceTimbreProfile::SubBassPad;
	float StereoPanPosition = 0.0f;
	float SlotGain = 1.0f;

	float Envelope = 0.0f;
	float PhaseAngle = 0.0f;
	float PitchDecay = 0.0f;

	inline void Trigger()
	{
		Envelope = 1.0f;
		PitchDecay = 1.0f;
		PhaseAngle = 0.0f;
	}

	float GenerateSample(float SampleRate, float ChordMultiplier, float& OutLeftSample, float& OutRightSample);
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THEPITPROJECT_API UHarmonicConvergenceSynthComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	UHarmonicConvergenceSynthComponent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Audio")
	bool bMonoMode = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SubBassDroneVolume = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Audio", meta = (ClampMin = "100.0", ClampMax = "2000.0"))
	float FilterBaselineCutoffHz = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Audio", meta = (ClampMin = "0.5", ClampMax = "3.0"))
	float FilterResonanceQ = 0.85f;

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Synth")
	void NoteOn(int32 VoiceIndex, float NoteFrequencyHz, float StereoPan = 0.0f);

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Synth")
	void NoteOff(int32 VoiceIndex);

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Synth")
	void SetVoiceModulation(int32 VoiceIndex, float ModulationIntensity);

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Synth")
	void SetConvergenceEnergy(float Energy);

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Synth")
	void TriggerAttractPing(float ChimeFrequencyHz);

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Synth")
	void TriggerVoiceIgnition(int32 VoiceIndex);

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Synth")
	void SetOverdriveIntensity(float InIntensity);

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Synth")
	void TriggerOverdriveDrop();

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Synth")
	void TriggerEchoRipple(float ChimeFrequencyHz, float StereoPan = 0.0f);

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Synth")
	void SetRhythmicPulseSlot(int32 SlotIndex, bool bActive, int32 RhythmicMask = 0x1111, float NoteFrequencyHz = 130.81f, float StereoPan = 0.0f, EVoiceTimbreProfile TimbreProfile = EVoiceTimbreProfile::SubBassPad, float Gain = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Synth")
	void ClearAllRhythmicPulseSlots();

	UFUNCTION(BlueprintPure, Category = "Harmonics|Synth")
	float GetMasterBPM() const { return MasterTempoBPM; }

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Synth")
	void SetMasterBPM(float NewBPM) { MasterTempoBPM = FMath::Clamp(NewBPM, 40.0f, 200.0f); }

	UFUNCTION(BlueprintPure, Category = "Harmonics|Synth")
	int32 GetCurrentBeatStep() const { return CurrentBeatStep; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Audio")
	bool bEnableChordProgression = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Audio", meta = (ClampMin = "1", ClampMax = "16"))
	int32 BarsPerChord = 4;

	UFUNCTION(BlueprintPure, Category = "Harmonics|Synth")
	int32 GetCurrentChordIndex() const { return CurrentChordIndex; }

	UFUNCTION(BlueprintPure, Category = "Harmonics|Synth")
	float GetCurrentChordMultiplier() const { return CurrentChordMultiplier; }

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	static constexpr int32 MaxVoices = 24;
	FConvergenceVoiceDSP Voices[MaxVoices];

	static constexpr int32 MaxRhythmicSlots = 24;
	FRhythmicPulseDSP RhythmicSlots[MaxRhythmicSlots];

	float MasterTempoBPM = 120.0f;
	float BeatClockPhase = 0.0f;
	int32 CurrentBeatStep = 0;

	float RootSubBassDronePhaseAngle = 0.0f;
	float TargetConvergenceEnergy = 0.0f;
	float CurrentConvergenceEnergy = 0.0f;

	float AttractChimeEnvelope = 0.0f;
	float AttractChimePhaseAngle = 0.0f;
	float AttractChimeFrequencyHz = 523.25f;

	float OverdriveIntensity = 0.0f;
	float OverdriveDropEnvelope = 0.0f;
	float OverdriveDropPhaseAngle = 0.0f;

	float EchoRippleEnvelope = 0.0f;
	float EchoRipplePhaseAngle = 0.0f;
	float EchoRippleFrequencyHz = 880.0f;
	float EchoRipplePanPosition = 0.0f;

	int32 CurrentMeasureIndex = 0;
	int32 CurrentChordIndex = 0;
	float TargetChordMultiplier = 1.0f;
	float CurrentChordMultiplier = 1.0f;

	FConvergenceResonantFilter LeftChannelFilter;
	FConvergenceResonantFilter RightChannelFilter;

	FAmbientDiffusionTank DiffusionTank;
	float CurrentSampleRate = 48000.0f;

	FCriticalSection AudioLock;
};
