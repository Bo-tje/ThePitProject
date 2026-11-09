#include "HarmonicConvergenceSynthComponent.h"

float FConvergenceVoiceDSP::GenerateSample(float SampleRate, float ChordMultiplier, float& OutLeftSample, float& OutRightSample)
{
	if (!bIsVoiceActive && AmplitudeEnvelope <= 0.0005f && TransientEnvelope <= 0.0005f && IgnitionEnvelope <= 0.0005f)
	{
		OutLeftSample = 0.0f;
		OutRightSample = 0.0f;
		return 0.0f;
	}

	const float InvSampleRate = 1.0f / SampleRate;

	if (bIsVoiceActive)
	{
		AmplitudeEnvelope = FMath::FInterpTo(AmplitudeEnvelope, 1.0f, InvSampleRate, 16.0f);
	}
	else
	{
		AmplitudeEnvelope = FMath::FInterpTo(AmplitudeEnvelope, 0.0f, InvSampleRate, 12.0f);
	}

	// Rising Energy Capacitor Sweep Frequency
	const float ChargeRatio = FMath::Clamp(HoldModulationIntensity, 0.0f, 1.0f);
	const float SweepMultiplier = 1.0f + 0.85f * FMath::Pow(ChargeRatio, 1.5f);
	const float CurrentBeamFreqHz = NoteFrequencyHz * SweepMultiplier * ChordMultiplier;

	// Electric flutter modulation (14 Hz plasma shimmer)
	FlutterPhaseAngle += (14.0f * 2.0f * PI) * InvSampleRate;
	if (FlutterPhaseAngle > 2.0f * PI) FlutterPhaseAngle -= 2.0f * PI;
	const float ElectricFlutter = 1.0f + 0.15f * FMath::Sin(FlutterPhaseAngle);

	CarrierPhaseAngle += (CurrentBeamFreqHz * 2.0f * PI) * InvSampleRate;
	if (CarrierPhaseAngle > 2.0f * PI) CarrierPhaseAngle -= 2.0f * PI;

	HarmonicPhaseAngle += (CurrentBeamFreqHz * 2.0f * 2.0f * PI) * InvSampleRate;
	if (HarmonicPhaseAngle > 2.0f * PI) HarmonicPhaseAngle -= 2.0f * PI;

	const float PlasmaFundamental = FMath::Sin(CarrierPhaseAngle);
	const float PlasmaHarmonic = FMath::Sin(HarmonicPhaseAngle) * 0.35f;

	const float DuckingFactor = (ChargeRatio >= 0.99f) ? 0.15f : 1.0f;
	const float ChargingGain = (0.15f + 0.30f * ChargeRatio) * DuckingFactor;

	float MonoVoiceSample = (PlasmaFundamental * 0.7f + PlasmaHarmonic * 0.3f) * ElectricFlutter * ChargingGain * AmplitudeEnvelope;

	// Tactile Note-On Drum Pad Bite (~70ms crisp percussive transient)
	if (TransientEnvelope > 0.001f)
	{
		TransientEnvelope = FMath::FInterpTo(TransientEnvelope, 0.0f, InvSampleRate, 22.0f);
		TransientPhaseAngle += (NoteFrequencyHz * 3.0f * 2.0f * PI) * InvSampleRate;
		if (TransientPhaseAngle > 2.0f * PI) TransientPhaseAngle -= 2.0f * PI;

		const float TactileBite = (FMath::Sin(TransientPhaseAngle) * 0.65f + FMath::Sin(TransientPhaseAngle * 1.5f) * 0.35f) * TransientEnvelope * 0.50f;
		MonoVoiceSample += TactileBite;
	}

	// Crystal Ignition Flash Strike (~250ms glass chime on 100% full charge)
	if (IgnitionEnvelope > 0.001f)
	{
		IgnitionEnvelope = FMath::FInterpTo(IgnitionEnvelope, 0.0f, InvSampleRate, 6.0f);
		IgnitionPhaseAngle += (NoteFrequencyHz * 2.5f * 2.0f * PI) * InvSampleRate;
		if (IgnitionPhaseAngle > 2.0f * PI) IgnitionPhaseAngle -= 2.0f * PI;

		const float IgnitionChime = (FMath::Sin(IgnitionPhaseAngle) * 0.7f + FMath::Sin(IgnitionPhaseAngle * 2.76f) * 0.3f) * IgnitionEnvelope * 0.55f;
		MonoVoiceSample += IgnitionChime;
	}

	const float PanAngle = (StereoPanPosition + 1.0f) * 0.25f * PI;
	OutLeftSample = MonoVoiceSample * FMath::Cos(PanAngle);
	OutRightSample = MonoVoiceSample * FMath::Sin(PanAngle);

	return MonoVoiceSample;
}

float FRhythmicPulseDSP::GenerateSample(float SampleRate, float ChordMultiplier, float& OutLeftSample, float& OutRightSample)
{
	if (Envelope <= 0.0001f)
	{
		OutLeftSample = 0.0f;
		OutRightSample = 0.0f;
		return 0.0f;
	}

	const float InvSampleRate = 1.0f / SampleRate;
	float MonoSample = 0.0f;

	switch (TimbreProfile)
	{
	case EVoiceTimbreProfile::SubBassPad:
		{
			Envelope = FMath::FInterpTo(Envelope, 0.0f, InvSampleRate, 5.5f);
			PitchDecay = FMath::FInterpTo(PitchDecay, 0.0f, InvSampleRate, 26.0f);

			const float KickFreqHz = 50.0f + 60.0f * PitchDecay;
			PhaseAngle += (KickFreqHz * 2.0f * PI) * InvSampleRate;
			if (PhaseAngle > 2.0f * PI) PhaseAngle -= 2.0f * PI;

			const float SubSine = FMath::Sin(PhaseAngle);
			const float SubHarmonic = FMath::Sin(PhaseAngle * 2.0f) * 0.15f;
			MonoSample = (SubSine + SubHarmonic) * Envelope * SlotGain * 0.50f;
		}
		break;

	case EVoiceTimbreProfile::CrystallineChime:
		{
			Envelope = FMath::FInterpTo(Envelope, 0.0f, InvSampleRate, 12.0f);
			PhaseAngle += (BaseFrequencyHz * 1.5f * 2.0f * PI) * InvSampleRate;
			if (PhaseAngle > 2.0f * PI) PhaseAngle -= 2.0f * PI;

			const float ChimeSample = FMath::Sin(PhaseAngle) * 0.65f + FMath::Sin(PhaseAngle * 2.76f) * 0.25f + FMath::Sin(PhaseAngle * 5.4f) * 0.10f;
			MonoSample = ChimeSample * Envelope * SlotGain * 0.28f;
		}
		break;

	case EVoiceTimbreProfile::VortexSweep:
		{
			Envelope = FMath::FInterpTo(Envelope, 0.0f, InvSampleRate, 5.0f);
			const float EffectiveFreq = BaseFrequencyHz * ChordMultiplier;

			PhaseAngle += (EffectiveFreq * 2.0f * PI) * InvSampleRate;
			if (PhaseAngle > 2.0f * PI) PhaseAngle -= 2.0f * PI;

			const float Swirl = FMath::Sin(PhaseAngle) * 0.7f + FMath::Sin(PhaseAngle * 1.5f) * 0.3f;
			MonoSample = Swirl * Envelope * SlotGain * 0.32f;
		}
		break;

	case EVoiceTimbreProfile::WarmPad:
	default:
		{
			Envelope = FMath::FInterpTo(Envelope, 0.0f, InvSampleRate, 9.0f);
			const float EffectiveFreq = BaseFrequencyHz * ChordMultiplier;

			PhaseAngle += (EffectiveFreq * 2.0f * PI) * InvSampleRate;
			if (PhaseAngle > 2.0f * PI) PhaseAngle -= 2.0f * PI;

			const float WoodSample = FMath::Sin(PhaseAngle) * 0.78f + FMath::Sin(PhaseAngle * 3.0f) * 0.22f;
			MonoSample = WoodSample * Envelope * SlotGain * 0.35f;
		}
		break;
	}

	const float PanAngle = (StereoPanPosition + 1.0f) * 0.25f * PI;
	OutLeftSample = MonoSample * FMath::Cos(PanAngle);
	OutRightSample = MonoSample * FMath::Sin(PanAngle);

	return MonoSample;
}

UHarmonicConvergenceSynthComponent::UHarmonicConvergenceSynthComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 2;
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

void UHarmonicConvergenceSynthComponent::NoteOn(int32 VoiceIndex, float NoteFrequencyHz, float StereoPan)
{
	FScopeLock Lock(&AudioLock);
	if (VoiceIndex >= 0 && VoiceIndex < MaxVoices)
	{
		Voices[VoiceIndex].NoteFrequencyHz = NoteFrequencyHz;
		Voices[VoiceIndex].StereoPanPosition = FMath::Clamp(StereoPan, -1.0f, 1.0f);
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

void UHarmonicConvergenceSynthComponent::TriggerVoiceIgnition(int32 VoiceIndex)
{
	FScopeLock Lock(&AudioLock);
	if (VoiceIndex >= 0 && VoiceIndex < MaxVoices)
	{
		Voices[VoiceIndex].TriggerIgnition();
	}
}

void UHarmonicConvergenceSynthComponent::SetOverdriveIntensity(float InIntensity)
{
	FScopeLock Lock(&AudioLock);
	OverdriveIntensity = FMath::Clamp(InIntensity, 0.0f, 1.0f);
}

void UHarmonicConvergenceSynthComponent::TriggerOverdriveDrop()
{
	FScopeLock Lock(&AudioLock);
	OverdriveDropEnvelope = 1.0f;
	OverdriveDropPhaseAngle = 0.0f;
}

void UHarmonicConvergenceSynthComponent::TriggerEchoRipple(float ChimeFrequencyHz, float StereoPan)
{
	FScopeLock Lock(&AudioLock);
	EchoRippleEnvelope = 1.0f;
	EchoRipplePhaseAngle = 0.0f;
	EchoRippleFrequencyHz = FMath::Clamp(ChimeFrequencyHz, 200.0f, 3500.0f);
	EchoRipplePanPosition = FMath::Clamp(StereoPan, -1.0f, 1.0f);
}

void UHarmonicConvergenceSynthComponent::SetRhythmicPulseSlot(int32 SlotIndex, bool bActive, int32 RhythmicMask, float NoteFrequencyHz, float StereoPan, EVoiceTimbreProfile TimbreProfile, float Gain)
{
	FScopeLock Lock(&AudioLock);
	if (SlotIndex >= 0 && SlotIndex < MaxRhythmicSlots)
	{
		RhythmicSlots[SlotIndex].bIsActive = bActive;
		RhythmicSlots[SlotIndex].RhythmicPatternMask = RhythmicMask;
		RhythmicSlots[SlotIndex].BaseFrequencyHz = NoteFrequencyHz;
		RhythmicSlots[SlotIndex].StereoPanPosition = FMath::Clamp(StereoPan, -1.0f, 1.0f);
		RhythmicSlots[SlotIndex].TimbreProfile = TimbreProfile;
		RhythmicSlots[SlotIndex].SlotGain = FMath::Clamp(Gain, 0.0f, 1.0f);
	}
}

void UHarmonicConvergenceSynthComponent::ClearAllRhythmicPulseSlots()
{
	FScopeLock Lock(&AudioLock);
	for (int32 SlotIndex = 0; SlotIndex < MaxRhythmicSlots; ++SlotIndex)
	{
		RhythmicSlots[SlotIndex].bIsActive = false;
		RhythmicSlots[SlotIndex].Envelope = 0.0f;
	}
}

int32 UHarmonicConvergenceSynthComponent::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	FScopeLock Lock(&AudioLock);

	const int32 NumFrames = NumSamples / NumChannels;
	const float InvSampleRate = 1.0f / CurrentSampleRate;

	int32 ActivePlayingVoiceCount = 0;
	float MaxHoldModulation = 0.0f;
	for (int32 VoiceIndex = 0; VoiceIndex < MaxVoices; ++VoiceIndex)
	{
		if (Voices[VoiceIndex].bIsVoiceActive || Voices[VoiceIndex].AmplitudeEnvelope > 0.001f)
		{
			ActivePlayingVoiceCount++;
			MaxHoldModulation = FMath::Max(MaxHoldModulation, Voices[VoiceIndex].HoldModulationIntensity);
		}
	}
	const float DynamicVoiceHeadroomScaling = (ActivePlayingVoiceCount > 0) ? (1.0f / FMath::Sqrt(static_cast<float>(ActivePlayingVoiceCount))) : 1.0f;

	for (int32 Frame = 0; Frame < NumFrames; ++Frame)
	{
		CurrentConvergenceEnergy = FMath::FInterpTo(CurrentConvergenceEnergy, TargetConvergenceEnergy, InvSampleRate, 3.5f);

		// Advance Master Beat Clock (16th-Note Subdivision Quantization: 4 steps per beat)
		const float StepIncrement = (MasterTempoBPM * 4.0f / 60.0f) * InvSampleRate;
		BeatClockPhase += StepIncrement;
		if (BeatClockPhase >= 1.0f)
		{
			BeatClockPhase -= 1.0f;
			CurrentBeatStep = (CurrentBeatStep + 1) % 16;

			if (CurrentBeatStep == 0)
			{
				CurrentMeasureIndex++;
				if (bEnableChordProgression && CurrentMeasureIndex >= FMath::Max(1, BarsPerChord))
				{
					CurrentMeasureIndex = 0;
					CurrentChordIndex = (CurrentChordIndex + 1) % 4;

					static constexpr float ChordScaleMultipliers[4] = {
						1.0000000f, // Am (Root)
						0.7937005f, // F  (-4 semitones)
						1.1892071f, // C  (+3 semitones)
						0.8908987f  // G  (-2 semitones)
					};
					TargetChordMultiplier = ChordScaleMultipliers[CurrentChordIndex];
				}
			}

			const int32 StepBit = (1 << CurrentBeatStep);
			for (int32 SlotIndex = 0; SlotIndex < MaxRhythmicSlots; ++SlotIndex)
			{
				if (RhythmicSlots[SlotIndex].bIsActive && (RhythmicSlots[SlotIndex].RhythmicPatternMask & StepBit))
				{
					RhythmicSlots[SlotIndex].Trigger();
				}
			}
		}

		CurrentChordMultiplier = FMath::FInterpTo(CurrentChordMultiplier, TargetChordMultiplier, InvSampleRate, 4.0f);

		float AccumulatedLeftChannel = 0.0f;
		float AccumulatedRightChannel = 0.0f;

		// 1. Synthesize Polyphonic Voices
		for (int32 VoiceIndex = 0; VoiceIndex < MaxVoices; ++VoiceIndex)
		{
			float VoiceLeftSample = 0.0f;
			float VoiceRightSample = 0.0f;
			Voices[VoiceIndex].GenerateSample(CurrentSampleRate, CurrentChordMultiplier, VoiceLeftSample, VoiceRightSample);
			AccumulatedLeftChannel += VoiceLeftSample * DynamicVoiceHeadroomScaling * 0.65f;
			AccumulatedRightChannel += VoiceRightSample * DynamicVoiceHeadroomScaling * 0.65f;
		}

		// 2. Central Grounding Root Sub-Bass Drone (65.4 Hz C2) + Overdrive Sub-Octave (32.7 Hz C1)
		RootSubBassDronePhaseAngle += (65.41f * 2.0f * PI) * InvSampleRate;
		if (RootSubBassDronePhaseAngle > 2.0f * PI) RootSubBassDronePhaseAngle -= 2.0f * PI;

		const float SubBassDroneSample = FMath::Sin(RootSubBassDronePhaseAngle) * (CurrentConvergenceEnergy * SubBassDroneVolume);
		const float OverdriveSubSample = FMath::Sin(RootSubBassDronePhaseAngle * 0.5f) * (OverdriveIntensity * 0.22f);
		AccumulatedLeftChannel += (SubBassDroneSample + OverdriveSubSample);
		AccumulatedRightChannel += (SubBassDroneSample + OverdriveSubSample);

		// 3. Persistent Rhythmic Beat Slots
		int32 ActiveRhythmicSlotCount = 0;
		for (int32 SlotIndex = 0; SlotIndex < MaxRhythmicSlots; ++SlotIndex)
		{
			if (RhythmicSlots[SlotIndex].Envelope > 0.0005f)
			{
				ActiveRhythmicSlotCount++;
			}
		}
		const float DynamicRhythmicHeadroom = (ActiveRhythmicSlotCount > 0) ? (1.0f / FMath::Sqrt(static_cast<float>(ActiveRhythmicSlotCount))) : 1.0f;

		for (int32 SlotIndex = 0; SlotIndex < MaxRhythmicSlots; ++SlotIndex)
		{
			if (RhythmicSlots[SlotIndex].Envelope > 0.0005f)
			{
				float PulseLeft = 0.0f;
				float PulseRight = 0.0f;
				RhythmicSlots[SlotIndex].GenerateSample(CurrentSampleRate, CurrentChordMultiplier, PulseLeft, PulseRight);
				AccumulatedLeftChannel += PulseLeft * DynamicRhythmicHeadroom;
				AccumulatedRightChannel += PulseRight * DynamicRhythmicHeadroom;
			}
		}

		// Harmonic Overdrive Sub-Drop Detonation Impact (120 Hz -> 35 Hz deep dive)
		if (OverdriveDropEnvelope > 0.001f)
		{
			OverdriveDropEnvelope = FMath::FInterpTo(OverdriveDropEnvelope, 0.0f, InvSampleRate, 1.4f);
			const float DropFreqHz = 35.0f + 85.0f * OverdriveDropEnvelope;
			OverdriveDropPhaseAngle += (DropFreqHz * 2.0f * PI) * InvSampleRate;
			if (OverdriveDropPhaseAngle > 2.0f * PI) OverdriveDropPhaseAngle -= 2.0f * PI;

			const float DropSample = FMath::Tanh(FMath::Sin(OverdriveDropPhaseAngle) * 1.6f) * OverdriveDropEnvelope * 0.45f;
			AccumulatedLeftChannel += DropSample;
			AccumulatedRightChannel += DropSample;
		}

		// Echo Ripple Crystalline Bell on crystal expiration
		if (EchoRippleEnvelope > 0.001f)
		{
			EchoRippleEnvelope = FMath::FInterpTo(EchoRippleEnvelope, 0.0f, InvSampleRate, 3.5f);
			EchoRipplePhaseAngle += (EchoRippleFrequencyHz * 2.0f * PI) * InvSampleRate;
			if (EchoRipplePhaseAngle > 2.0f * PI) EchoRipplePhaseAngle -= 2.0f * PI;

			const float RippleSample = (FMath::Sin(EchoRipplePhaseAngle) * 0.6f + FMath::Sin(EchoRipplePhaseAngle * 2.76f) * 0.3f + FMath::Sin(EchoRipplePhaseAngle * 5.4f) * 0.1f) * EchoRippleEnvelope * 0.32f;
			const float RipplePanAngle = (EchoRipplePanPosition + 1.0f) * 0.25f * PI;
			AccumulatedLeftChannel += RippleSample * FMath::Cos(RipplePanAngle);
			AccumulatedRightChannel += RippleSample * FMath::Sin(RipplePanAngle);
		}

		// 4. Ambient Attract Chime
		if (AttractChimeEnvelope > 0.001f)
		{
			AttractChimeEnvelope = FMath::FInterpTo(AttractChimeEnvelope, 0.0f, InvSampleRate, 0.65f);
			AttractChimePhaseAngle += (AttractChimeFrequencyHz * 2.0f * PI) * InvSampleRate;
			if (AttractChimePhaseAngle > 2.0f * PI) AttractChimePhaseAngle -= 2.0f * PI;

			const float ChimeSample = FMath::Sin(AttractChimePhaseAngle) * AttractChimeEnvelope * 0.25f;
			AccumulatedLeftChannel += ChimeSample;
			AccumulatedRightChannel += ChimeSample;
		}

		// 5. Ambient Stereo Diffusion Reverb Tank
		float ReverbWetLeftSample = 0.0f;
		float ReverbWetRightSample = 0.0f;
		const float ReverbFeedbackGain = 0.55f + 0.25f * FMath::Clamp(CurrentConvergenceEnergy * 0.4f, 0.0f, 1.0f) + OverdriveIntensity * 0.15f;
		DiffusionTank.Process(AccumulatedLeftChannel, AccumulatedRightChannel, ReverbFeedbackGain, ReverbWetLeftSample, ReverbWetRightSample);

		const float ReverbWetMixRatio = 0.25f + 0.35f * FMath::Clamp(CurrentConvergenceEnergy * 0.5f, 0.0f, 1.0f) + OverdriveIntensity * 0.20f;
		const float FilterInputLeft = AccumulatedLeftChannel * (1.0f - ReverbWetMixRatio * 0.5f) + ReverbWetLeftSample * ReverbWetMixRatio;
		const float FilterInputRight = AccumulatedRightChannel * (1.0f - ReverbWetMixRatio * 0.5f) + ReverbWetRightSample * ReverbWetMixRatio;

		// 6. Dynamic Resonant Low-Pass Filter
		const float FilterCutoffFrequencyHz = FMath::Clamp(FilterBaselineCutoffHz + MaxHoldModulation * 2400.0f + CurrentConvergenceEnergy * 5500.0f + OverdriveIntensity * 4500.0f, 150.0f, 18000.0f);
		const float FilteredOutputLeft = LeftChannelFilter.Process(FilterInputLeft, FilterCutoffFrequencyHz, FilterResonanceQ, CurrentSampleRate);
		const float FilteredOutputRight = RightChannelFilter.Process(FilterInputRight, FilterCutoffFrequencyHz, FilterResonanceQ, CurrentSampleRate);

		if (bMonoMode)
		{
			const float SaturatedMonoSample = FMath::Tanh((FilteredOutputLeft + FilteredOutputRight) * 0.7071f);
			OutAudio[Frame * 2] = SaturatedMonoSample;
			OutAudio[Frame * 2 + 1] = SaturatedMonoSample;
		}
		else
		{
			OutAudio[Frame * 2] = FMath::Tanh(FilteredOutputLeft);
			OutAudio[Frame * 2 + 1] = FMath::Tanh(FilteredOutputRight);
		}
	}

	return NumSamples;
}
