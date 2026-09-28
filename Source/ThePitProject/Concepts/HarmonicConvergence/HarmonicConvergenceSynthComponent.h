#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "HarmonicConvergenceTypes.h"
#include "HarmonicConvergenceSynthComponent.generated.h"

// Unconditionally Stable 2-Pole State-Variable Low-Pass Filter (Bilinear / Trapezoidal SVF)
struct FConvergenceResonantFilter
{
	float Ic1eq = 0.0f;
	float Ic2eq = 0.0f;

	inline float Process(float InSample, float CutoffHz, float Q, float SampleRate)
	{
		const float ClampedCutoff = FMath::Clamp(CutoffHz, 20.0f, SampleRate * 0.45f);
		const float SafeQ = FMath::Clamp(Q, 0.5f, 6.0f);

		const float G = FMath::Tan(PI * (ClampedCutoff / SampleRate));
		const float K = 1.0f / SafeQ;
		const float A1 = 1.0f / (1.0f + G * (G + K));
		const float A2 = G * A1;
		const float A3 = G * A2;

		const float V3 = InSample - Ic2eq;
		const float V1 = A1 * Ic1eq + A2 * V3;
		const float V2 = Ic2eq + A2 * Ic1eq + A3 * V3;

		Ic1eq = 2.0f * V1 - Ic1eq;
		Ic2eq = 2.0f * V2 - Ic2eq;

		if (!FMath::IsFinite(V2))
		{
			Reset();
			return 0.0f;
		}

		return V2;
	}

	inline void Reset()
	{
		Ic1eq = 0.0f;
		Ic2eq = 0.0f;
	}
};

// 4-Tap Stereo Diffusion Reverb Tank for cinematic ambient blending
struct FAmbientDiffusionTank
{
	static constexpr int32 DelaySize1 = 1447;
	static constexpr int32 DelaySize2 = 1693;
	static constexpr int32 DelaySize3 = 1979;
	static constexpr int32 DelaySize4 = 2243;

	float Buf1[DelaySize1] = {0.0f};
	float Buf2[DelaySize2] = {0.0f};
	float Buf3[DelaySize3] = {0.0f};
	float Buf4[DelaySize4] = {0.0f};

	int32 Idx1 = 0, Idx2 = 0, Idx3 = 0, Idx4 = 0;
	float Damp1 = 0.0f, Damp2 = 0.0f, Damp3 = 0.0f, Damp4 = 0.0f;

	inline void Process(float InLeft, float InRight, float Feedback, float& OutLeft, float& OutRight)
	{
		const float Fb = FMath::Clamp(Feedback, 0.0f, 0.88f);
		const float Input = (InLeft + InRight) * 0.35f;

		const float Out1 = Buf1[Idx1];
		Damp1 = Damp1 * 0.45f + Out1 * 0.55f;
		Buf1[Idx1] = Input + Damp1 * Fb;
		if (++Idx1 >= DelaySize1) Idx1 = 0;

		const float Out2 = Buf2[Idx2];
		Damp2 = Damp2 * 0.45f + Out2 * 0.55f;
		Buf2[Idx2] = Input + Damp2 * Fb;
		if (++Idx2 >= DelaySize2) Idx2 = 0;

		const float Out3 = Buf3[Idx3];
		Damp3 = Damp3 * 0.45f + Out3 * 0.55f;
		Buf3[Idx3] = Input + Damp3 * Fb;
		if (++Idx3 >= DelaySize3) Idx3 = 0;

		const float Out4 = Buf4[Idx4];
		Damp4 = Damp4 * 0.45f + Out4 * 0.55f;
		Buf4[Idx4] = Input + Damp4 * Fb;
		if (++Idx4 >= DelaySize4) Idx4 = 0;

		OutLeft = (Out1 - Out3) * 0.5f;
		OutRight = (Out2 - Out4) * 0.5f;
	}

	inline void Reset()
	{
		FMemory::Memzero(Buf1, sizeof(Buf1));
		FMemory::Memzero(Buf2, sizeof(Buf2));
		FMemory::Memzero(Buf3, sizeof(Buf3));
		FMemory::Memzero(Buf4, sizeof(Buf4));
		Idx1 = Idx2 = Idx3 = Idx4 = 0;
		Damp1 = Damp2 = Damp3 = Damp4 = 0.0f;
	}
};

// Multi-Tier Polyphonic Voice Engine
struct FConvergenceVoiceDSP
{
	float Frequency = 261.63f;
	float PhaseA = 0.0f;
	float PhaseB = 0.0f;
	float LfoPhase = 0.0f;
	float Modulation = 0.0f;
	float Pan = 0.0f; // -1.0 Left to +1.0 Right
	EVoiceTimbreProfile Profile = EVoiceTimbreProfile::WarmPad;

	// Envelope states
	bool bActive = false;
	float EnvValue = 0.0f;

	// Tactile Note-On Strike Transient (Fast 12ms attack bite)
	float TransientEnv = 0.0f;
	float TransientPhase = 0.0f;

	float GenerateSample(float SampleRate, float& OutLeft, float& OutRight);
	void NoteOn(EVoiceTimbreProfile InProfile)
	{
		Profile = InProfile;
		bActive = true;
		TransientEnv = 1.0f; // Fire instant attack transient on every button press
		TransientPhase = 0.0f;
	}
	void NoteOff() { bActive = false; }
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

	// --- Note Triggers ---
	UFUNCTION(BlueprintCallable, Category = "Harmonics|Synth")
	void NoteOn(int32 VoiceIndex, float FrequencyHz, float Pan = 0.0f, EVoiceTimbreProfile Profile = EVoiceTimbreProfile::WarmPad);

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Synth")
	void NoteOff(int32 VoiceIndex);

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Synth")
	void SetVoiceModulation(int32 VoiceIndex, float ModulationIntensity);

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Synth")
	void SetConvergenceEnergy(float Energy);

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Synth")
	void TriggerShockwave();

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Synth")
	void TriggerAttractPing(float FrequencyHz);

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	static constexpr int32 MaxVoices = 24; // Full 20-station support + headroom
	FConvergenceVoiceDSP Voices[MaxVoices];

	// Central Sub-Bass Drone (65.4 Hz C2)
	float DronePhase = 0.0f;
	float TargetConvergenceEnergy = 0.0f;
	float CurrentConvergenceEnergy = 0.0f;

	// Shockwave State
	float ShockwaveEnv = 0.0f;
	float ShockwavePhase = 0.0f;
	float ShockwaveFreq = 95.0f;

	// Attract Chime State
	float AttractEnv = 0.0f;
	float AttractPhase = 0.0f;
	float AttractFreq = 523.25f;

	// Resonant State-Variable Filters for Left and Right Channels
	FConvergenceResonantFilter LeftFilter;
	FConvergenceResonantFilter RightFilter;

	// Ambient Diffusion Tank
	FAmbientDiffusionTank DiffusionTank;
	float CurrentSampleRate = 48000.0f;

	FCriticalSection AudioLock;
};
