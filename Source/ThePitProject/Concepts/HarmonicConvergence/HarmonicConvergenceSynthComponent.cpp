#include "HarmonicConvergenceSynthComponent.h"

float FConvergenceVoiceDSP::GenerateSample(float SampleRate, float& OutLeft, float& OutRight)
{
	if (EnvValue <= 0.0001f && !bActive)
	{
		OutLeft = 0.0f;
		OutRight = 0.0f;
		return 0.0f;
	}

	float MonoVoice = 0.0f;
	const float InvSampleRate = 1.0f / SampleRate;

	switch (Profile)
	{
	case EVoiceTimbreProfile::SubBassPad:
		{
			// Deep Grounding Sub-Bass Anchor (0.15s attack, 2.2s release)
			if (bActive)
			{
				EnvValue = FMath::FInterpTo(EnvValue, 1.0f, InvSampleRate, 7.0f);
			}
			else
			{
				EnvValue = FMath::FInterpTo(EnvValue, 0.0f, InvSampleRate, 0.65f);
			}

			PhaseA += (Frequency * 2.0f * PI) * InvSampleRate;
			if (PhaseA > 2.0f * PI) PhaseA -= 2.0f * PI;

			// Pure deep sine with gentle 2nd harmonic octave
			const float Sub = FMath::Sin(PhaseA);
			const float SubOct = FMath::Sin(PhaseA * 2.0f) * 0.20f;
			MonoVoice = (Sub + SubOct) * EnvValue * 1.25f;
		}
		break;

	case EVoiceTimbreProfile::CrystallineChime:
		{
			// Sparkling High Chime / Glass Pluck (Fast 0.015s strike, 3.5s ring-out)
			if (bActive)
			{
				EnvValue = FMath::FInterpTo(EnvValue, 1.0f, InvSampleRate, 25.0f); // Instant strike
			}
			else
			{
				EnvValue = FMath::FInterpTo(EnvValue, 0.0f, InvSampleRate, 0.35f); // 3.5s natural decay
			}

			PhaseA += (Frequency * 2.0f * PI) * InvSampleRate;
			if (PhaseA > 2.0f * PI) PhaseA -= 2.0f * PI;

			PhaseB += (Frequency * 2.001f * 2.0f * PI) * InvSampleRate;
			if (PhaseB > 2.0f * PI) PhaseB -= 2.0f * PI;

			// Crystalline harmonics (Fundamental + 2nd + 4th + 8th shimmer)
			const float Chime1 = FMath::Sin(PhaseA);
			const float Chime2 = FMath::Sin(PhaseB * 2.0f) * 0.35f;
			const float Chime4 = FMath::Sin(PhaseA * 4.0f) * 0.12f;
			MonoVoice = (Chime1 + Chime2 + Chime4) * EnvValue * 0.60f;
		}
		break;

	case EVoiceTimbreProfile::VortexSweep:
		{
			// Resonant Vortex Drone (0.4s attack, 2.5s release, slow phase modulation)
			if (bActive)
			{
				EnvValue = FMath::FInterpTo(EnvValue, 1.0f, InvSampleRate, 4.0f);
			}
			else
			{
				EnvValue = FMath::FInterpTo(EnvValue, 0.0f, InvSampleRate, 0.50f);
			}

			const float LfoInc = (1.5f * 2.0f * PI) * InvSampleRate;
			LfoPhase += LfoInc;
			if (LfoPhase > 2.0f * PI) LfoPhase -= 2.0f * PI;

			const float ModHz = Frequency + FMath::Sin(LfoPhase) * 12.0f;
			PhaseA += (ModHz * 2.0f * PI) * InvSampleRate;
			if (PhaseA > 2.0f * PI) PhaseA -= 2.0f * PI;

			const float Vortex = FMath::Sin(PhaseA) + FMath::Sin(PhaseA * 1.5f) * 0.3f;
			MonoVoice = Vortex * EnvValue * 0.75f;
		}
		break;

	case EVoiceTimbreProfile::WarmPad:
	default:
		{
			// Warm Dual-Detuned Pad (0.2s attack, 1.8s ambient release)
			if (bActive)
			{
				EnvValue = FMath::FInterpTo(EnvValue, 1.0f, InvSampleRate, 6.0f);
			}
			else
			{
				EnvValue = FMath::FInterpTo(EnvValue, 0.0f, InvSampleRate, 0.85f);
			}

			// Subtle Analog Vibrato (4.5 Hz)
			const float LfoInc = (4.5f * 2.0f * PI) * InvSampleRate;
			LfoPhase += LfoInc;
			if (LfoPhase > 2.0f * PI) LfoPhase -= 2.0f * PI;
			const float VibratoHz = FMath::Sin(LfoPhase) * (Modulation * 3.5f);

			const float BaseFreq = FMath::Max(20.0f, Frequency + VibratoHz);
			const float DetuneFreq = BaseFreq * 1.0022f; // +3.8 cents detune

			PhaseA += (BaseFreq * 2.0f * PI) * InvSampleRate;
			if (PhaseA > 2.0f * PI) PhaseA -= 2.0f * PI;

			PhaseB += (DetuneFreq * 2.0f * PI) * InvSampleRate;
			if (PhaseB > 2.0f * PI) PhaseB -= 2.0f * PI;

			const float OscA = FMath::Sin(PhaseA) + FMath::Sin(PhaseA * 2.0f) * 0.18f;
			const float OscB = FMath::Sin(PhaseB) + FMath::Sin(PhaseB * 2.0f) * 0.18f;
			MonoVoice = (OscA + OscB) * 0.5f * EnvValue * 0.80f;
		}
		break;
	}

	// Instant Tactile Strike Transient (Fast 12ms attack bite on Note-On)
	if (TransientEnv > 0.001f)
	{
		TransientEnv = FMath::FInterpTo(TransientEnv, 0.0f, InvSampleRate, 50.0f); // Fast ~12ms decay
		TransientPhase += (Frequency * 2.5f * 2.0f * PI) * InvSampleRate;
		if (TransientPhase > 2.0f * PI) TransientPhase -= 2.0f * PI;

		const float ClickPluck = FMath::Sin(TransientPhase) * TransientEnv * 0.35f;
		MonoVoice += ClickPluck;
	}

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

void UHarmonicConvergenceSynthComponent::NoteOn(int32 VoiceIndex, float FrequencyHz, float Pan, EVoiceTimbreProfile Profile)
{
	FScopeLock Lock(&AudioLock);
	if (VoiceIndex >= 0 && VoiceIndex < MaxVoices)
	{
		Voices[VoiceIndex].Frequency = FrequencyHz;
		Voices[VoiceIndex].Pan = FMath::Clamp(Pan, -1.0f, 1.0f);
		Voices[VoiceIndex].NoteOn(Profile);
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

	// Count active playing voices for dynamic 1 / sqrt(N) power scaling
	int32 ActiveCount = 0;
	for (int32 i = 0; i < MaxVoices; ++i)
	{
		if (Voices[i].bActive || Voices[i].EnvValue > 0.001f)
		{
			ActiveCount++;
		}
	}
	const float VoiceScaling = (ActiveCount > 0) ? (1.0f / FMath::Sqrt((float)ActiveCount)) : 1.0f;

	for (int32 Frame = 0; Frame < NumFrames; ++Frame)
	{
		CurrentConvergenceEnergy = FMath::FInterpTo(CurrentConvergenceEnergy, TargetConvergenceEnergy, InvSampleRate, 3.5f);

		float LeftSum = 0.0f;
		float RightSum = 0.0f;

		// 1. Synthesize all 20+ Polyphonic Voices with dynamic gain staging
		for (int32 i = 0; i < MaxVoices; ++i)
		{
			float VLeft = 0.0f;
			float VRight = 0.0f;
			Voices[i].GenerateSample(CurrentSampleRate, VLeft, VRight);
			LeftSum += VLeft * VoiceScaling * 0.65f;
			RightSum += VRight * VoiceScaling * 0.65f;
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

		// 5. Ambient Stereo Diffusion Reverb Tank
		float DiffLeft = 0.0f;
		float DiffRight = 0.0f;
		const float ReverbFeedback = 0.55f + 0.25f * FMath::Clamp(CurrentConvergenceEnergy * 0.4f, 0.0f, 1.0f);
		DiffusionTank.Process(LeftSum, RightSum, ReverbFeedback, DiffLeft, DiffRight);

		// Blend Dry + Reverb as convergence energy builds
		const float WetMix = 0.25f + 0.35f * FMath::Clamp(CurrentConvergenceEnergy * 0.5f, 0.0f, 1.0f);
		float MixedLeft = LeftSum * (1.0f - WetMix * 0.5f) + DiffLeft * WetMix;
		float MixedRight = RightSum * (1.0f - WetMix * 0.5f) + DiffRight * WetMix;

		// 6. Dynamic Resonant Low-Pass Filter
		const float Cutoff = FMath::Clamp(220.0f + CurrentConvergenceEnergy * 5500.0f, 150.0f, 16000.0f);
		const float Q = 1.4f;

		const float FilteredLeft = LeftFilter.Process(MixedLeft, Cutoff, Q, CurrentSampleRate);
		const float FilteredRight = RightFilter.Process(MixedRight, Cutoff, Q, CurrentSampleRate);

		// Output handling: Mono Downmix vs. Spatial Stereo
		if (bMonoMode)
		{
			// Equal-power sum to guarantee 100% loudness and zero phase-drop on a single physical speaker
			const float MonoDownmix = (FilteredLeft + FilteredRight) * 0.7071f;
			const float Saturated = FMath::Tanh(MonoDownmix);
			OutAudio[Frame * 2] = Saturated;
			OutAudio[Frame * 2 + 1] = Saturated;
		}
		else
		{
			// Spatial Stereo for multi-speaker setups
			OutAudio[Frame * 2] = FMath::Tanh(FilteredLeft);
			OutAudio[Frame * 2 + 1] = FMath::Tanh(FilteredRight);
		}
	}

	return NumSamples;
}
