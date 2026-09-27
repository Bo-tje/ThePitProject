#include "HarmonicConvergenceSynthComponent.h"

float FConvergenceVoiceDSP::GenerateSample(float SampleRate, float& OutLeft, float& OutRight)
{
	if (EnvValue <= 0.0001f && !bActive)
	{
		OutLeft = 0.0f;
		OutRight = 0.0f;
		return 0.0f;
	}

	// Smooth Organic ADSR Envelope
	if (bActive)
	{
		EnvValue = FMath::FInterpTo(EnvValue, 1.0f, 1.0f / SampleRate, 6.0f); // Fast smooth attack ~0.15s
	}
	else
	{
		EnvValue = FMath::FInterpTo(EnvValue, 0.0f, 1.0f / SampleRate, 0.85f); // Long 1.8s ambient release
	}

	// Subtle Analog Vibrato (4.5 Hz)
	const float LfoInc = (4.5f * 2.0f * PI) / SampleRate;
	LfoPhase += LfoInc;
	if (LfoPhase > 2.0f * PI)
	{
		LfoPhase -= 2.0f * PI;
	}
	const float VibratoHz = FMath::Sin(LfoPhase) * (Modulation * 3.5f);

	// Dual Detuned Warm Oscillators (Primary + 4 Cents Detune for stereo lushness)
	const float BaseFreq = FMath::Max(20.0f, Frequency + VibratoHz);
	const float DetuneFreq = BaseFreq * 1.0022f; // +3.8 cents detune

	PhaseA += (BaseFreq * 2.0f * PI) / SampleRate;
	if (PhaseA > 2.0f * PI) PhaseA -= 2.0f * PI;

	PhaseB += (DetuneFreq * 2.0f * PI) / SampleRate;
	if (PhaseB > 2.0f * PI) PhaseB -= 2.0f * PI;

	// Warm rounded waveform (Pure Sine + Gentle 2nd Harmonic Octave)
	const float OscA = FMath::Sin(PhaseA) + FMath::Sin(PhaseA * 2.0f) * 0.18f;
	const float OscB = FMath::Sin(PhaseB) + FMath::Sin(PhaseB * 2.0f) * 0.18f;

	const float MonoVoice = (OscA + OscB) * 0.5f * EnvValue;

	// Equal-power stereo spatial panning
	const float PanAngle = (Pan + 1.0f) * 0.25f * PI;
	OutLeft = MonoVoice * FMath::Cos(PanAngle);
	OutRight = MonoVoice * FMath::Sin(PanAngle);

	return MonoVoice;
}

UHarmonicConvergenceSynthComponent::UHarmonicConvergenceSynthComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 2; // Stereo Output
	bAutoActivate = true;
}

bool UHarmonicConvergenceSynthComponent::Init(int32& SampleRate)
{
	NumChannels = 2;
	CurrentSampleRate = SampleRate > 0 ? (float)SampleRate : 48000.0f;

	LeftFilter.Reset();
	RightFilter.Reset();
	DiffusionTank.Reset();

	return true;
}

void UHarmonicConvergenceSynthComponent::NoteOn(int32 VoiceIndex, float FrequencyHz, float Pan)
{
	FScopeLock Lock(&AudioLock);
	if (VoiceIndex >= 0 && VoiceIndex < MaxVoices)
	{
		Voices[VoiceIndex].Frequency = FrequencyHz;
		Voices[VoiceIndex].Pan = FMath::Clamp(Pan, -1.0f, 1.0f);
		Voices[VoiceIndex].NoteOn();
	}
}

void UHarmonicConvergenceSynthComponent::NoteOff(int32 VoiceIndex)
{
	FScopeLock Lock(&AudioLock);
	if (VoiceIndex >= 0 && VoiceIndex < MaxVoices)
	{
		Voices[VoiceIndex].NoteOff();
	}
}

void UHarmonicConvergenceSynthComponent::SetVoiceModulation(int32 VoiceIndex, float ModulationIntensity)
{
	FScopeLock Lock(&AudioLock);
	if (VoiceIndex >= 0 && VoiceIndex < MaxVoices)
	{
		Voices[VoiceIndex].Modulation = FMath::Clamp(ModulationIntensity, 0.0f, 1.0f);
	}
}

void UHarmonicConvergenceSynthComponent::SetConvergenceEnergy(float Energy)
{
	TargetConvergenceEnergy = Energy;
}

void UHarmonicConvergenceSynthComponent::TriggerShockwave()
{
	FScopeLock Lock(&AudioLock);
	ShockwaveEnv = 1.0f;
	ShockwaveFreq = 95.0f;
}

void UHarmonicConvergenceSynthComponent::TriggerAttractPing(float FrequencyHz)
{
	FScopeLock Lock(&AudioLock);
	AttractEnv = 1.0f;
	AttractFreq = FrequencyHz;
}

int32 UHarmonicConvergenceSynthComponent::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	FScopeLock Lock(&AudioLock);

	const int32 NumFrames = NumSamples / NumChannels;
	const float InvSampleRate = 1.0f / CurrentSampleRate;

	for (int32 Frame = 0; Frame < NumFrames; ++Frame)
	{
		CurrentConvergenceEnergy = FMath::FInterpTo(CurrentConvergenceEnergy, TargetConvergenceEnergy, InvSampleRate, 3.5f);

		float LeftSum = 0.0f;
		float RightSum = 0.0f;

		// 1. Synthesize Dual-Detuned Polyphonic Voices
		for (int32 i = 0; i < MaxVoices; ++i)
		{
			float VLeft = 0.0f;
			float VRight = 0.0f;
			Voices[i].GenerateSample(CurrentSampleRate, VLeft, VRight);
			LeftSum += VLeft * 0.40f;
			RightSum += VRight * 0.40f;
		}

		// 2. Central Grounding Root Sub-Bass Drone (65.4 Hz C2)
		const float DroneInc = (65.41f * 2.0f * PI) * InvSampleRate;
		DronePhase += DroneInc;
		if (DronePhase > 2.0f * PI) DronePhase -= 2.0f * PI;

		const float SubDrone = FMath::Sin(DronePhase) * (CurrentConvergenceEnergy * 0.28f);
		LeftSum += SubDrone;
		RightSum += SubDrone;

		// 3. Shockwave Sub-Bass Impact (95Hz -> 30Hz)
		if (ShockwaveEnv > 0.001f)
		{
			ShockwaveEnv = FMath::FInterpTo(ShockwaveEnv, 0.0f, InvSampleRate, 1.0f);
			ShockwaveFreq = FMath::FInterpTo(ShockwaveFreq, 30.0f, InvSampleRate, 2.5f);

			const float ShockwaveInc = (ShockwaveFreq * 2.0f * PI) * InvSampleRate;
			ShockwavePhase += ShockwaveInc;
			if (ShockwavePhase > 2.0f * PI) ShockwavePhase -= 2.0f * PI;

			const float ShockSample = FMath::Sin(ShockwavePhase) * ShockwaveEnv * 0.5f;
			LeftSum += ShockSample;
			RightSum += ShockSample;
		}

		// 4. Ambient Attract Chime
		if (AttractEnv > 0.001f)
		{
			AttractEnv = FMath::FInterpTo(AttractEnv, 0.0f, InvSampleRate, 0.65f);
			const float AttractInc = (AttractFreq * 2.0f * PI) * InvSampleRate;
			AttractPhase += AttractInc;
			if (AttractPhase > 2.0f * PI) AttractPhase -= 2.0f * PI;

			const float ChimeSample = FMath::Sin(AttractPhase) * AttractEnv * 0.25f;
			LeftSum += ChimeSample;
			RightSum += ChimeSample;
		}

		// 5. Ambient Stereo Diffusion Reverb Tank (Blends multiple voices into a single unified wash)
		float DiffLeft = 0.0f;
		float DiffRight = 0.0f;
		const float ReverbFeedback = 0.55f + 0.25f * FMath::Clamp(CurrentConvergenceEnergy * 0.5f, 0.0f, 1.0f);
		DiffusionTank.Process(LeftSum, RightSum, ReverbFeedback, DiffLeft, DiffRight);

		// Mix Dry + Diffused Reverb based on convergence
		const float WetMix = 0.25f + 0.35f * FMath::Clamp(CurrentConvergenceEnergy * 0.6f, 0.0f, 1.0f);
		float MixedLeft = LeftSum * (1.0f - WetMix * 0.5f) + DiffLeft * WetMix;
		float MixedRight = RightSum * (1.0f - WetMix * 0.5f) + DiffRight * WetMix;

		// 6. Dynamic Resonant Low-Pass Filter (Opens smoothly as voices converge)
		const float Cutoff = FMath::Clamp(220.0f + CurrentConvergenceEnergy * 5500.0f, 150.0f, 16000.0f);
		const float Q = 1.4f; // Musical warmth without harsh whistle peaks

		const float FilteredLeft = LeftFilter.Process(MixedLeft, Cutoff, Q, CurrentSampleRate);
		const float FilteredRight = RightFilter.Process(MixedRight, Cutoff, Q, CurrentSampleRate);

		// Soft-clipping saturation limiter (Tanh)
		OutAudio[Frame * 2] = FMath::Tanh(FilteredLeft);
		OutAudio[Frame * 2 + 1] = FMath::Tanh(FilteredRight);
	}

	return NumSamples;
}
