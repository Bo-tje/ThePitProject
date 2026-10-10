#include "HarmonicConvergenceSynthComponent.h"

float FConvergenceVoiceDSP::GenerateSample(float SampleRate, float& OutLeftSample, float& OutRightSample)
{
	if (AmplitudeEnvelope <= 0.0001f && !bIsVoiceActive)
	{
		OutLeftSample = 0.0f;
		OutRightSample = 0.0f;
		return 0.0f;
	}

	float MonoVoiceSample = 0.0f;
	const float InvSampleRate = 1.0f / SampleRate;

	switch (TimbreProfile)
	{
	case EVoiceTimbreProfile::SubBassPad:
		{
			// Deep Grounding Sub-Bass Anchor (0.15s attack, 2.2s release)
			if (bIsVoiceActive)
			{
				AmplitudeEnvelope = FMath::FInterpTo(AmplitudeEnvelope, 1.0f, InvSampleRate, 7.0f);
			}
			else
			{
				AmplitudeEnvelope = FMath::FInterpTo(AmplitudeEnvelope, 0.0f, InvSampleRate, 0.65f);
			}

			CarrierPhaseAngle += (NoteFrequencyHz * 2.0f * PI) * InvSampleRate;
			if (CarrierPhaseAngle > 2.0f * PI) CarrierPhaseAngle -= 2.0f * PI;

			// Pure deep sine with gentle 2nd harmonic octave
			const float FundamentalSine = FMath::Sin(CarrierPhaseAngle);
			const float SecondHarmonicOctave = FMath::Sin(CarrierPhaseAngle * 2.0f) * 0.15f;
			MonoVoiceSample = (FundamentalSine + SecondHarmonicOctave) * AmplitudeEnvelope * 0.65f;
		}
		break;

	case EVoiceTimbreProfile::CrystallineChime:
		{
			// Sparkling High Chime / Glass Pluck (Fast 0.015s strike, 3.5s ring-out)
			if (bIsVoiceActive)
			{
				AmplitudeEnvelope = FMath::FInterpTo(AmplitudeEnvelope, 1.0f, InvSampleRate, 25.0f); // Instant strike
			}
			else
			{
				AmplitudeEnvelope = FMath::FInterpTo(AmplitudeEnvelope, 0.0f, InvSampleRate, 0.35f); // 3.5s natural decay
			}

			CarrierPhaseAngle += (NoteFrequencyHz * 2.0f * PI) * InvSampleRate;
			if (CarrierPhaseAngle > 2.0f * PI) CarrierPhaseAngle -= 2.0f * PI;

			HarmonicPhaseAngle += (NoteFrequencyHz * 2.001f * 2.0f * PI) * InvSampleRate;
			if (HarmonicPhaseAngle > 2.0f * PI) HarmonicPhaseAngle -= 2.0f * PI;

			// Crystalline harmonics (Fundamental + 2nd + 4th shimmer)
			const float ChimeFundamental = FMath::Sin(CarrierPhaseAngle);
			const float ChimeSecondHarmonic = FMath::Sin(HarmonicPhaseAngle * 2.0f) * 0.35f;
			const float ChimeFourthHarmonic = FMath::Sin(CarrierPhaseAngle * 4.0f) * 0.12f;
			MonoVoiceSample = (ChimeFundamental + ChimeSecondHarmonic + ChimeFourthHarmonic) * AmplitudeEnvelope * 0.60f;
		}
		break;

	case EVoiceTimbreProfile::VortexSweep:
		{
			// Resonant Vortex Drone (0.4s attack, 2.5s release, slow phase modulation)
			if (bIsVoiceActive)
			{
				AmplitudeEnvelope = FMath::FInterpTo(AmplitudeEnvelope, 1.0f, InvSampleRate, 4.0f);
			}
			else
			{
				AmplitudeEnvelope = FMath::FInterpTo(AmplitudeEnvelope, 0.0f, InvSampleRate, 0.50f);
			}

			const float LfoPhaseIncrement = (1.5f * 2.0f * PI) * InvSampleRate;
			VibratoLfoPhaseAngle += LfoPhaseIncrement;
			if (VibratoLfoPhaseAngle > 2.0f * PI) VibratoLfoPhaseAngle -= 2.0f * PI;

			const float ModulatedFrequencyHz = NoteFrequencyHz + FMath::Sin(VibratoLfoPhaseAngle) * 12.0f;
			CarrierPhaseAngle += (ModulatedFrequencyHz * 2.0f * PI) * InvSampleRate;
			if (CarrierPhaseAngle > 2.0f * PI) CarrierPhaseAngle -= 2.0f * PI;

			const float VortexFundamental = FMath::Sin(CarrierPhaseAngle);
			const float VortexFifthHarmonic = FMath::Sin(CarrierPhaseAngle * 1.5f) * 0.3f;
			MonoVoiceSample = (VortexFundamental + VortexFifthHarmonic) * AmplitudeEnvelope * 0.75f;
		}
		break;

	case EVoiceTimbreProfile::WarmPad:
	default:
		{
			// Warm Dual-Detuned Pad (0.2s attack, 1.8s ambient release)
			if (bIsVoiceActive)
			{
				AmplitudeEnvelope = FMath::FInterpTo(AmplitudeEnvelope, 1.0f, InvSampleRate, 6.0f);
			}
			else
			{
				AmplitudeEnvelope = FMath::FInterpTo(AmplitudeEnvelope, 0.0f, InvSampleRate, 0.85f);
			}

			// Subtle Analog Vibrato (4.5 Hz) modulated by hold time
			const float LfoPhaseIncrement = (4.5f * 2.0f * PI) * InvSampleRate;
			VibratoLfoPhaseAngle += LfoPhaseIncrement;
			if (VibratoLfoPhaseAngle > 2.0f * PI) VibratoLfoPhaseAngle -= 2.0f * PI;
			const float VibratoOffsetHz = FMath::Sin(VibratoLfoPhaseAngle) * (HoldModulationIntensity * 3.5f);

			const float BaseCarrierFreqHz = FMath::Max(20.0f, NoteFrequencyHz + VibratoOffsetHz);
			const float DetunedCarrierFreqHz = BaseCarrierFreqHz * 1.0022f; // +3.8 cents detune

			CarrierPhaseAngle += (BaseCarrierFreqHz * 2.0f * PI) * InvSampleRate;
			if (CarrierPhaseAngle > 2.0f * PI) CarrierPhaseAngle -= 2.0f * PI;

			HarmonicPhaseAngle += (DetunedCarrierFreqHz * 2.0f * PI) * InvSampleRate;
			if (HarmonicPhaseAngle > 2.0f * PI) HarmonicPhaseAngle -= 2.0f * PI;

			const float OscillatorA = FMath::Sin(CarrierPhaseAngle) + FMath::Sin(CarrierPhaseAngle * 2.0f) * 0.18f;
			const float OscillatorB = FMath::Sin(HarmonicPhaseAngle) + FMath::Sin(HarmonicPhaseAngle * 2.0f) * 0.18f;
			MonoVoiceSample = (OscillatorA + OscillatorB) * 0.5f * AmplitudeEnvelope * 0.80f;
		}
		break;
	}

	// Instant Tactile Strike Transient (Fast 12ms attack bite on Note-On)
	if (TransientEnvelope > 0.001f)
	{
		TransientEnvelope = FMath::FInterpTo(TransientEnvelope, 0.0f, InvSampleRate, 50.0f); // Fast ~12ms decay
		TransientPhaseAngle += (NoteFrequencyHz * 2.5f * 2.0f * PI) * InvSampleRate;
		if (TransientPhaseAngle > 2.0f * PI) TransientPhaseAngle -= 2.0f * PI;

		const float AttackClickPluck = FMath::Sin(TransientPhaseAngle) * TransientEnvelope * 0.35f;
		MonoVoiceSample += AttackClickPluck;
	}

	// Equal-power stereo spatial panning
	const float PanAngle = (StereoPanPosition + 1.0f) * 0.25f * PI;
	OutLeftSample = MonoVoiceSample * FMath::Cos(PanAngle);
	OutRightSample = MonoVoiceSample * FMath::Sin(PanAngle);

	return MonoVoiceSample;
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
	CurrentSampleRate = SampleRate > 0 ? static_cast<float>(SampleRate) : 48000.0f;

	LeftChannelFilter.Reset();
	RightChannelFilter.Reset();
	DiffusionTank.Reset();

	return true;
}

void UHarmonicConvergenceSynthComponent::NoteOn(int32 VoiceIndex, float NoteFrequencyHz, float StereoPan, EVoiceTimbreProfile TimbreProfile)
{
	FScopeLock Lock(&AudioLock);
	if (VoiceIndex >= 0 && VoiceIndex < MaxVoices)
	{
		Voices[VoiceIndex].NoteFrequencyHz = NoteFrequencyHz;
		Voices[VoiceIndex].StereoPanPosition = FMath::Clamp(StereoPan, -1.0f, 1.0f);
		Voices[VoiceIndex].NoteOn(TimbreProfile);
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
		Voices[VoiceIndex].HoldModulationIntensity = FMath::Clamp(ModulationIntensity, 0.0f, 1.0f);
	}
}

void UHarmonicConvergenceSynthComponent::SetConvergenceEnergy(float Energy)
{
	TargetConvergenceEnergy = Energy;
}



void UHarmonicConvergenceSynthComponent::TriggerAttractPing(float ChimeFrequencyHz)
{
	FScopeLock Lock(&AudioLock);
	AttractChimeEnvelope = 1.0f;
	AttractChimeFrequencyHz = ChimeFrequencyHz;
}

int32 UHarmonicConvergenceSynthComponent::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	FScopeLock Lock(&AudioLock);

	const int32 NumFrames = NumSamples / NumChannels;
	const float InvSampleRate = 1.0f / CurrentSampleRate;

	// Count active playing voices for dynamic 1 / sqrt(N) power scaling
	int32 ActivePlayingVoiceCount = 0;
	for (int32 VoiceIndex = 0; VoiceIndex < MaxVoices; ++VoiceIndex)
	{
		if (Voices[VoiceIndex].bIsVoiceActive || Voices[VoiceIndex].AmplitudeEnvelope > 0.001f)
		{
			ActivePlayingVoiceCount++;
		}
	}
	const float DynamicVoiceHeadroomScaling = (ActivePlayingVoiceCount > 0) ? (1.0f / FMath::Sqrt(static_cast<float>(ActivePlayingVoiceCount))) : 1.0f;

	for (int32 Frame = 0; Frame < NumFrames; ++Frame)
	{
		CurrentConvergenceEnergy = FMath::FInterpTo(CurrentConvergenceEnergy, TargetConvergenceEnergy, InvSampleRate, 3.5f);

		float AccumulatedLeftChannel = 0.0f;
		float AccumulatedRightChannel = 0.0f;

		// 1. Synthesize all 20+ Polyphonic Voices with dynamic gain staging
		for (int32 VoiceIndex = 0; VoiceIndex < MaxVoices; ++VoiceIndex)
		{
			float VoiceLeftSample = 0.0f;
			float VoiceRightSample = 0.0f;
			Voices[VoiceIndex].GenerateSample(CurrentSampleRate, VoiceLeftSample, VoiceRightSample);
			AccumulatedLeftChannel += VoiceLeftSample * DynamicVoiceHeadroomScaling * 0.65f;
			AccumulatedRightChannel += VoiceRightSample * DynamicVoiceHeadroomScaling * 0.65f;
		}

		// 2. Central Grounding Root Sub-Bass Drone (65.4 Hz C2)
		const float DronePhaseIncrement = (65.41f * 2.0f * PI) * InvSampleRate;
		RootSubBassDronePhaseAngle += DronePhaseIncrement;
		if (RootSubBassDronePhaseAngle > 2.0f * PI) RootSubBassDronePhaseAngle -= 2.0f * PI;

		const float SubBassDroneSample = FMath::Sin(RootSubBassDronePhaseAngle) * (CurrentConvergenceEnergy * SubBassDroneVolume);
		AccumulatedLeftChannel += SubBassDroneSample;
		AccumulatedRightChannel += SubBassDroneSample;



		// 4. Ambient Attract Chime
		if (AttractChimeEnvelope > 0.001f)
		{
			AttractChimeEnvelope = FMath::FInterpTo(AttractChimeEnvelope, 0.0f, InvSampleRate, 0.65f);
			const float AttractPhaseIncrement = (AttractChimeFrequencyHz * 2.0f * PI) * InvSampleRate;
			AttractChimePhaseAngle += AttractPhaseIncrement;
			if (AttractChimePhaseAngle > 2.0f * PI) AttractChimePhaseAngle -= 2.0f * PI;

			const float ChimeSample = FMath::Sin(AttractChimePhaseAngle) * AttractChimeEnvelope * 0.25f;
			AccumulatedLeftChannel += ChimeSample;
			AccumulatedRightChannel += ChimeSample;
		}

		// 5. Ambient Stereo Diffusion Reverb Tank
		float ReverbWetLeftSample = 0.0f;
		float ReverbWetRightSample = 0.0f;
		const float ReverbFeedbackGain = 0.55f + 0.25f * FMath::Clamp(CurrentConvergenceEnergy * 0.4f, 0.0f, 1.0f);
		DiffusionTank.Process(AccumulatedLeftChannel, AccumulatedRightChannel, ReverbFeedbackGain, ReverbWetLeftSample, ReverbWetRightSample);

		// Blend Dry + Reverb as convergence energy builds
		const float ReverbWetMixRatio = 0.25f + 0.35f * FMath::Clamp(CurrentConvergenceEnergy * 0.5f, 0.0f, 1.0f);
		float FilterInputLeft = AccumulatedLeftChannel * (1.0f - ReverbWetMixRatio * 0.5f) + ReverbWetLeftSample * ReverbWetMixRatio;
		float FilterInputRight = AccumulatedRightChannel * (1.0f - ReverbWetMixRatio * 0.5f) + ReverbWetRightSample * ReverbWetMixRatio;

		// 6. Dynamic Resonant Low-Pass Filter (Cutoff sweeps from FilterBaselineCutoffHz to 16 kHz with energy)
		const float FilterCutoffFrequencyHz = FMath::Clamp(FilterBaselineCutoffHz + CurrentConvergenceEnergy * 5500.0f, 150.0f, 16000.0f);
		const float ResonanceQ = FilterResonanceQ;

		const float FilteredOutputLeft = LeftChannelFilter.Process(FilterInputLeft, FilterCutoffFrequencyHz, ResonanceQ, CurrentSampleRate);
		const float FilteredOutputRight = RightChannelFilter.Process(FilterInputRight, FilterCutoffFrequencyHz, ResonanceQ, CurrentSampleRate);

		// Output handling: Mono Downmix vs. Spatial Stereo
		if (bMonoMode)
		{
			// Equal-power sum to guarantee 100% loudness and zero phase-drop on a single physical speaker
			const float MonoDownmix = (FilteredOutputLeft + FilteredOutputRight) * 0.7071f;
			const float SaturatedMonoSample = FMath::Tanh(MonoDownmix);
			OutAudio[Frame * 2] = SaturatedMonoSample;
			OutAudio[Frame * 2 + 1] = SaturatedMonoSample;
		}
		else
		{
			// Spatial Stereo for multi-speaker setups
			OutAudio[Frame * 2] = FMath::Tanh(FilteredOutputLeft);
			OutAudio[Frame * 2 + 1] = FMath::Tanh(FilteredOutputRight);
		}
	}

	return NumSamples;
}
