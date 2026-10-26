#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "HarmonicConvergenceTypes.h"
#include "HarmonicConvergenceSynthComponent.generated.h"

// Unconditionally Stable 2-Pole State-Variable Low-Pass Filter (Bilinear / Trapezoidal SVF)
struct FConvergenceResonantFilter
{
	float IntegratorState1 = 0.0f; // Integrator 1 state memory (Ic1eq)
	float IntegratorState2 = 0.0f; // Integrator 2 state memory (Ic2eq)

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
	static constexpr int32 DelayBufferSizeTap1 = 1447;
	static constexpr int32 DelayBufferSizeTap2 = 1693;
	static constexpr int32 DelayBufferSizeTap3 = 1979;
	static constexpr int32 DelayBufferSizeTap4 = 2243;

	float DelayBufferTap1[DelayBufferSizeTap1] = {0.0f};
	float DelayBufferTap2[DelayBufferSizeTap2] = {0.0f};
	float DelayBufferTap3[DelayBufferSizeTap3] = {0.0f};
	float DelayBufferTap4[DelayBufferSizeTap4] = {0.0f};

	int32 WriteIndexTap1 = 0;
	int32 WriteIndexTap2 = 0;
	int32 WriteIndexTap3 = 0;
	int32 WriteIndexTap4 = 0;

	float LowPassDampingTap1 = 0.0f;
	float LowPassDampingTap2 = 0.0f;
	float LowPassDampingTap3 = 0.0f;
	float LowPassDampingTap4 = 0.0f;

	inline void Process(float InLeftSample, float InRightSample, float FeedbackGain, float& OutLeftSample, float& OutRightSample)
	{
		const float ClampedFeedbackGain = FMath::Clamp(FeedbackGain, 0.0f, 0.88f);
		const float MonoInputSample = (InLeftSample + InRightSample) * 0.35f;

		// Tap 1
		const float DelayedSampleTap1 = DelayBufferTap1[WriteIndexTap1];
		LowPassDampingTap1 = LowPassDampingTap1 * 0.45f + DelayedSampleTap1 * 0.55f;
		DelayBufferTap1[WriteIndexTap1] = MonoInputSample + LowPassDampingTap1 * ClampedFeedbackGain;
		if (++WriteIndexTap1 >= DelayBufferSizeTap1) WriteIndexTap1 = 0;

		// Tap 2
		const float DelayedSampleTap2 = DelayBufferTap2[WriteIndexTap2];
		LowPassDampingTap2 = LowPassDampingTap2 * 0.45f + DelayedSampleTap2 * 0.55f;
		DelayBufferTap2[WriteIndexTap2] = MonoInputSample + LowPassDampingTap2 * ClampedFeedbackGain;
		if (++WriteIndexTap2 >= DelayBufferSizeTap2) WriteIndexTap2 = 0;

		// Tap 3
		const float DelayedSampleTap3 = DelayBufferTap3[WriteIndexTap3];
		LowPassDampingTap3 = LowPassDampingTap3 * 0.45f + DelayedSampleTap3 * 0.55f;
		DelayBufferTap3[WriteIndexTap3] = MonoInputSample + LowPassDampingTap3 * ClampedFeedbackGain;
		if (++WriteIndexTap3 >= DelayBufferSizeTap3) WriteIndexTap3 = 0;

		// Tap 4
		const float DelayedSampleTap4 = DelayBufferTap4[WriteIndexTap4];
		LowPassDampingTap4 = LowPassDampingTap4 * 0.45f + DelayedSampleTap4 * 0.55f;
		DelayBufferTap4[WriteIndexTap4] = MonoInputSample + LowPassDampingTap4 * ClampedFeedbackGain;
		if (++WriteIndexTap4 >= DelayBufferSizeTap4) WriteIndexTap4 = 0;

		// Cross-coupled stereo output
		OutLeftSample = (DelayedSampleTap1 - DelayedSampleTap3) * 0.5f;
		OutRightSample = (DelayedSampleTap2 - DelayedSampleTap4) * 0.5f;
	}

	inline void Reset()
	{
		FMemory::Memzero(DelayBufferTap1, sizeof(DelayBufferTap1));
		FMemory::Memzero(DelayBufferTap2, sizeof(DelayBufferTap2));
		FMemory::Memzero(DelayBufferTap3, sizeof(DelayBufferTap3));
		FMemory::Memzero(DelayBufferTap4, sizeof(DelayBufferTap4));
		WriteIndexTap1 = WriteIndexTap2 = WriteIndexTap3 = WriteIndexTap4 = 0;
		LowPassDampingTap1 = LowPassDampingTap2 = LowPassDampingTap3 = LowPassDampingTap4 = 0.0f;
	}
};

// Multi-Tier Polyphonic Voice Engine
struct FConvergenceVoiceDSP
{
	float NoteFrequencyHz = 261.63f;
	float CarrierPhaseAngle = 0.0f;
	float HarmonicPhaseAngle = 0.0f;
	float VibratoLfoPhaseAngle = 0.0f;
	float HoldModulationIntensity = 0.0f;
	float StereoPanPosition = 0.0f; // -1.0 Left to +1.0 Right
	EVoiceTimbreProfile TimbreProfile = EVoiceTimbreProfile::WarmPad;

	// Envelope states
	bool bIsVoiceActive = false;
	float AmplitudeEnvelope = 0.0f;

	// Tactile Note-On Strike Transient (Fast 12ms attack bite)
	float TransientEnvelope = 0.0f;
	float TransientPhaseAngle = 0.0f;

	float GenerateSample(float SampleRate, float& OutLeftSample, float& OutRightSample);
	void NoteOn(EVoiceTimbreProfile InTimbreProfile)
	{
		TimbreProfile = InTimbreProfile;
		bIsVoiceActive = true;
		TransientEnvelope = 1.0f; // Fire instant attack transient on every button press
		TransientPhaseAngle = 0.0f;
	}
	void NoteOff()
	{
		bIsVoiceActive = false;
	}
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THEPITPROJECT_API UHarmonicConvergenceSynthComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	UHarmonicConvergenceSynthComponent(const FObjectInitializer& ObjectInitializer);

	// When true, sums audio into an equal-power mono downmix (ideal for single-speaker setups)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Audio")
	bool bMonoMode = true;

	// Master volume scalar for the central 65.4 Hz sub-bass drone (reduced from previous harsh defaults)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SubBassDroneVolume = 0.08f;

	// Baseline filter cutoff in Hz when idle/solo (raised from 220Hz to 350Hz for cleaner mids)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Audio", meta = (ClampMin = "100.0", ClampMax = "2000.0"))
	float FilterBaselineCutoffHz = 350.0f;

	// Filter resonance Q factor (lowered from 1.4 to 0.85 to eliminate boomy low-end resonance)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Audio", meta = (ClampMin = "0.5", ClampMax = "3.0"))
	float FilterResonanceQ = 0.85f;

	// --- Note Triggers ---
	UFUNCTION(BlueprintCallable, Category = "Harmonics|Synth")
	void NoteOn(int32 VoiceIndex, float NoteFrequencyHz, float StereoPan = 0.0f, EVoiceTimbreProfile TimbreProfile = EVoiceTimbreProfile::WarmPad);

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Synth")
	void NoteOff(int32 VoiceIndex);

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Synth")
	void SetVoiceModulation(int32 VoiceIndex, float ModulationIntensity);

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Synth")
	void SetConvergenceEnergy(float Energy);

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Synth")
	void TriggerAttractPing(float ChimeFrequencyHz);

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	static constexpr int32 MaxVoices = 24; // Full 20-station support + headroom
	FConvergenceVoiceDSP Voices[MaxVoices];

	// Central Sub-Bass Drone (65.4 Hz C2)
	float RootSubBassDronePhaseAngle = 0.0f;
	float TargetConvergenceEnergy = 0.0f;
	float CurrentConvergenceEnergy = 0.0f;



	// Attract Chime State
	float AttractChimeEnvelope = 0.0f;
	float AttractChimePhaseAngle = 0.0f;
	float AttractChimeFrequencyHz = 523.25f;

	// Resonant State-Variable Filters for Left and Right Channels
	FConvergenceResonantFilter LeftChannelFilter;
	FConvergenceResonantFilter RightChannelFilter;

	// Ambient Diffusion Tank
	FAmbientDiffusionTank DiffusionTank;
	float CurrentSampleRate = 48000.0f;

	FCriticalSection AudioLock;
};
