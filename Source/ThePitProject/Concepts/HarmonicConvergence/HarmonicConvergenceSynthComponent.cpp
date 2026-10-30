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

	// 1. Snappy Envelope: fast 60ms rise on button press, clean 120ms cutoff on release (no lingering drones)
	if (bIsVoiceActive)
	{
		AmplitudeEnvelope = FMath::FInterpTo(AmplitudeEnvelope, 1.0f, InvSampleRate, 16.0f);
	}
	else
	{
		AmplitudeEnvelope = FMath::FInterpTo(AmplitudeEnvelope, 0.0f, InvSampleRate, 12.0f);
	}

	// 2. Rising Energy Capacitor Sweep Frequency
	// Base frequency sweeps smoothly upward by +85% as beam pours energy into crystal, modulated by chord key
	const float ChargeRatio = FMath::Clamp(HoldModulationIntensity, 0.0f, 1.0f);
	const float SweepMultiplier = 1.0f + 0.85f * FMath::Pow(ChargeRatio, 1.5f);
	const float CurrentBeamFreqHz = NoteFrequencyHz * SweepMultiplier * ChordMultiplier;

	// Electric flutter modulation (14 Hz plasma shimmer)
	const float FlutterIncrement = (14.0f * 2.0f * PI) * InvSampleRate;
	FlutterPhaseAngle += FlutterIncrement;
	if (FlutterPhaseAngle > 2.0f * PI) FlutterPhaseAngle -= 2.0f * PI;
	const float ElectricFlutter = 1.0f + 0.15f * FMath::Sin(FlutterPhaseAngle);

	CarrierPhaseAngle += (CurrentBeamFreqHz * 2.0f * PI) * InvSampleRate;
	if (CarrierPhaseAngle > 2.0f * PI) CarrierPhaseAngle -= 2.0f * PI;

	HarmonicPhaseAngle += (CurrentBeamFreqHz * 2.0f * 2.0f * PI) * InvSampleRate;
	if (HarmonicPhaseAngle > 2.0f * PI) HarmonicPhaseAngle -= 2.0f * PI;

	// Focused Resonant Plasma Sound: fundamental + sparkling 2nd harmonic
	const float PlasmaFundamental = FMath::Sin(CarrierPhaseAngle);
	const float PlasmaHarmonic = FMath::Sin(HarmonicPhaseAngle) * 0.35f;

	// Dynamic Charging Gain: builds from gentle hum (0.15) to energetic beam (0.45)
	// Once crystal reaches full charge (1.0), beam ducks to 0.12 so the crystal drum rhythm dominates!
	const float DuckingFactor = (ChargeRatio >= 0.99f) ? 0.15f : 1.0f;
	const float ChargingGain = (0.15f + 0.30f * ChargeRatio) * DuckingFactor;

	float MonoVoiceSample = (PlasmaFundamental * 0.7f + PlasmaHarmonic * 0.3f) * ElectricFlutter * ChargingGain * AmplitudeEnvelope;

	// 3. Tactile Note-On Drum Pad Bite (~70ms crisp percussive transient)
	if (TransientEnvelope > 0.001f)
	{
		TransientEnvelope = FMath::FInterpTo(TransientEnvelope, 0.0f, InvSampleRate, 22.0f); // ~70ms bite
		TransientPhaseAngle += (NoteFrequencyHz * 3.0f * 2.0f * PI) * InvSampleRate;
		if (TransientPhaseAngle > 2.0f * PI) TransientPhaseAngle -= 2.0f * PI;

		const float TactileBite = (FMath::Sin(TransientPhaseAngle) * 0.65f + FMath::Sin(TransientPhaseAngle * 1.5f) * 0.35f) * TransientEnvelope * 0.50f;
		MonoVoiceSample += TactileBite;
	}

	// 4. Crystal Ignition Flash Strike (~250ms glass chime on 100% full charge)
	if (IgnitionEnvelope > 0.001f)
	{
		IgnitionEnvelope = FMath::FInterpTo(IgnitionEnvelope, 0.0f, InvSampleRate, 6.0f); // ~250ms ring
		IgnitionPhaseAngle += (NoteFrequencyHz * 2.5f * 2.0f * PI) * InvSampleRate;
		if (IgnitionPhaseAngle > 2.0f * PI) IgnitionPhaseAngle -= 2.0f * PI;

		const float IgnitionChime = (FMath::Sin(IgnitionPhaseAngle) * 0.7f + FMath::Sin(IgnitionPhaseAngle * 2.76f) * 0.3f) * IgnitionEnvelope * 0.55f;
		MonoVoiceSample += IgnitionChime;
	}

	// Equal-power stereo spatial panning
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
			if (bIsOvercharged)
			{
				// Overcharge: aggressive 808 club punch with saturated drive
				Envelope = FMath::FInterpTo(Envelope, 0.0f, InvSampleRate, 3.8f);
				PitchDecay = FMath::FInterpTo(PitchDecay, 0.0f, InvSampleRate, 18.0f);
				const float KickFreqHz = 55.0f + 95.0f * PitchDecay;
				PhaseAngle += (KickFreqHz * 2.0f * PI) * InvSampleRate;
				if (PhaseAngle > 2.0f * PI) PhaseAngle -= 2.0f * PI;

				const float SubSine = FMath::Sin(PhaseAngle);
				const float Saturated = FMath::Tanh(SubSine * 1.5f);
				MonoSample = Saturated * Envelope * SlotGain * 0.65f;
			}
			else
			{
				// Heartbeat Sub-Kick: fast pitch dive (110Hz -> 50Hz) and punchy 180ms decay
				Envelope = FMath::FInterpTo(Envelope, 0.0f, InvSampleRate, 5.5f);
				PitchDecay = FMath::FInterpTo(PitchDecay, 0.0f, InvSampleRate, 26.0f);

				const float KickFreqHz = 50.0f + 60.0f * PitchDecay;
				PhaseAngle += (KickFreqHz * 2.0f * PI) * InvSampleRate;
				if (PhaseAngle > 2.0f * PI) PhaseAngle -= 2.0f * PI;

				const float SubSine = FMath::Sin(PhaseAngle);
				const float SubHarmonic = FMath::Sin(PhaseAngle * 2.0f) * 0.15f;
				MonoSample = (SubSine + SubHarmonic) * Envelope * SlotGain * 0.50f;
			}
		}
		break;

	case EVoiceTimbreProfile::CrystallineChime:
		{
			if (bIsOvercharged)
			{
				// Overcharge: rapid 32nd-note ratchet / snare roll fill
				Envelope = FMath::FInterpTo(Envelope, 0.0f, InvSampleRate, 22.0f);
				PhaseAngle += (BaseFrequencyHz * 2.2f * 2.0f * PI) * InvSampleRate;
				if (PhaseAngle > 2.0f * PI) PhaseAngle -= 2.0f * PI;

				const float ChimeSample = FMath::Sin(PhaseAngle) * 0.7f + FMath::Sin(PhaseAngle * 3.14f) * 0.3f;
				MonoSample = ChimeSample * Envelope * SlotGain * 0.42f;
			}
			else
			{
				// Glass Ping / Crisp Clock Tick: high frequency chime with 80ms decay
				Envelope = FMath::FInterpTo(Envelope, 0.0f, InvSampleRate, 12.0f);

				PhaseAngle += (BaseFrequencyHz * 1.5f * 2.0f * PI) * InvSampleRate;
				if (PhaseAngle > 2.0f * PI) PhaseAngle -= 2.0f * PI;

				const float ChimeSample = FMath::Sin(PhaseAngle) * 0.65f + FMath::Sin(PhaseAngle * 2.76f) * 0.25f + FMath::Sin(PhaseAngle * 5.4f) * 0.10f;
				MonoSample = ChimeSample * Envelope * SlotGain * 0.28f;
			}
		}
		break;

	case EVoiceTimbreProfile::VortexSweep:
		{
			// Swirling Breath Pulse: gentle resonant swoosh with 200ms decay, transposed by chord
			Envelope = FMath::FInterpTo(Envelope, 0.0f, InvSampleRate, 5.0f);
			const float EffectiveFreq = BaseFrequencyHz * ChordMultiplier * (bIsOvercharged ? 1.5f : 1.0f);

			PhaseAngle += (EffectiveFreq * 2.0f * PI) * InvSampleRate;
			if (PhaseAngle > 2.0f * PI) PhaseAngle -= 2.0f * PI;

			const float Swirl = FMath::Sin(PhaseAngle) * 0.7f + FMath::Sin(PhaseAngle * 1.5f) * 0.3f;
			MonoSample = Swirl * Envelope * SlotGain * (bIsOvercharged ? 0.45f : 0.32f);
		}
		break;

	case EVoiceTimbreProfile::WarmPad:
	default:
		{
			// Wooden Marimba / Kalimba Ping: warm organic click with 110ms decay, transposed by chord
			Envelope = FMath::FInterpTo(Envelope, 0.0f, InvSampleRate, bIsOvercharged ? 6.0f : 9.0f);
			const float EffectiveFreq = BaseFrequencyHz * ChordMultiplier * (bIsOvercharged ? 2.0f : 1.0f);

			PhaseAngle += (EffectiveFreq * 2.0f * PI) * InvSampleRate;
			if (PhaseAngle > 2.0f * PI) PhaseAngle -= 2.0f * PI;

			const float WoodSample = FMath::Sin(PhaseAngle) * 0.78f + FMath::Sin(PhaseAngle * 3.0f) * 0.22f;
			MonoSample = WoodSample * Envelope * SlotGain * (bIsOvercharged ? 0.50f : 0.35f);
		}
		break;
	}

	// Equal-power stereo spatial panning
	const float PanAngle = (StereoPanPosition + 1.0f) * 0.25f * PI;
	OutLeftSample = MonoSample * FMath::Cos(PanAngle);
	OutRightSample = MonoSample * FMath::Sin(PanAngle);

	return MonoSample;
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

void UHarmonicConvergenceSynthComponent::TriggerVoiceIgnition(int32 VoiceIndex)
{
	FScopeLock Lock(&AudioLock);
	if (VoiceIndex >= 0 && VoiceIndex < MaxVoices)
	{
		Voices[VoiceIndex].TriggerIgnition();
	}
}

void UHarmonicConvergenceSynthComponent::SetSlotOvercharge(int32 SlotIndex, bool bOvercharge)
{
	FScopeLock Lock(&AudioLock);
	if (SlotIndex >= 0 && SlotIndex < MaxRhythmicSlots)
	{
		RhythmicSlots[SlotIndex].bIsOvercharged = bOvercharge;
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

	// Count active playing voices for dynamic 1 / sqrt(N) power scaling and peak hold modulation
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

			// Advance measure counter at the start of each 16-step bar (Step 0)
			if (CurrentBeatStep == 0)
			{
				CurrentMeasureIndex++;
				if (bEnableChordProgression && CurrentMeasureIndex >= FMath::Max(1, BarsPerChord))
				{
					CurrentMeasureIndex = 0;
					CurrentChordIndex = (CurrentChordIndex + 1) % 4;

					// 4-Chord Progression (i -> VI -> III -> VII in A minor: Am, F, C, G)
					static const float ChordScaleMultipliers[4] = {
						1.0000000f, // Am (Root)
						0.7937005f, // F  (-4 semitones: 2^(-4/12))
						1.1892071f, // C  (+3 semitones: 2^(3/12))
						0.8908987f  // G  (-2 semitones: 2^(-2/12))
					};
					TargetChordMultiplier = ChordScaleMultipliers[CurrentChordIndex];
				}
			}

			const int32 StepBit = (1 << CurrentBeatStep);
			for (int32 SlotIndex = 0; SlotIndex < MaxRhythmicSlots; ++SlotIndex)
			{
				if (RhythmicSlots[SlotIndex].bIsActive)
				{
					// Overcharge fills: if overcharged, combine pattern with rapid 8th-note ratchets (0x5555)
					const int32 ActivePattern = RhythmicSlots[SlotIndex].bIsOvercharged ? (RhythmicSlots[SlotIndex].RhythmicPatternMask | 0x5555) : RhythmicSlots[SlotIndex].RhythmicPatternMask;
					if (ActivePattern & StepBit)
					{
						RhythmicSlots[SlotIndex].Trigger();
					}
				}
			}
		}

		// Smooth chord transposition to prevent audio clicks
		CurrentChordMultiplier = FMath::FInterpTo(CurrentChordMultiplier, TargetChordMultiplier, InvSampleRate, 4.0f);

		float AccumulatedLeftChannel = 0.0f;
		float AccumulatedRightChannel = 0.0f;

		// 1. Synthesize all 20+ Polyphonic Voices with dynamic gain staging
		for (int32 VoiceIndex = 0; VoiceIndex < MaxVoices; ++VoiceIndex)
		{
			float VoiceLeftSample = 0.0f;
			float VoiceRightSample = 0.0f;
			Voices[VoiceIndex].GenerateSample(CurrentSampleRate, CurrentChordMultiplier, VoiceLeftSample, VoiceRightSample);
			AccumulatedLeftChannel += VoiceLeftSample * DynamicVoiceHeadroomScaling * 0.65f;
			AccumulatedRightChannel += VoiceRightSample * DynamicVoiceHeadroomScaling * 0.65f;
		}

		// 2. Central Grounding Root Sub-Bass Drone (65.4 Hz C2) + Overdrive Sub-Octave (32.7 Hz C1)
		const float DronePhaseIncrement = (65.41f * 2.0f * PI) * InvSampleRate;
		RootSubBassDronePhaseAngle += DronePhaseIncrement;
		if (RootSubBassDronePhaseAngle > 2.0f * PI) RootSubBassDronePhaseAngle -= 2.0f * PI;

		const float SubBassDroneSample = FMath::Sin(RootSubBassDronePhaseAngle) * (CurrentConvergenceEnergy * SubBassDroneVolume);
		const float OverdriveSubSample = FMath::Sin(RootSubBassDronePhaseAngle * 0.5f) * (OverdriveIntensity * 0.22f);
		AccumulatedLeftChannel += (SubBassDroneSample + OverdriveSubSample);
		AccumulatedRightChannel += (SubBassDroneSample + OverdriveSubSample);

		// 3. Persistent Rhythmic Beat Slots with dynamic polyphonic headroom scaling
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
		const float ReverbFeedbackGain = 0.55f + 0.25f * FMath::Clamp(CurrentConvergenceEnergy * 0.4f, 0.0f, 1.0f) + OverdriveIntensity * 0.15f;
		DiffusionTank.Process(AccumulatedLeftChannel, AccumulatedRightChannel, ReverbFeedbackGain, ReverbWetLeftSample, ReverbWetRightSample);

		// Blend Dry + Reverb as convergence energy and overdrive build
		const float ReverbWetMixRatio = 0.25f + 0.35f * FMath::Clamp(CurrentConvergenceEnergy * 0.5f, 0.0f, 1.0f) + OverdriveIntensity * 0.20f;
		float FilterInputLeft = AccumulatedLeftChannel * (1.0f - ReverbWetMixRatio * 0.5f) + ReverbWetLeftSample * ReverbWetMixRatio;
		float FilterInputRight = AccumulatedRightChannel * (1.0f - ReverbWetMixRatio * 0.5f) + ReverbWetRightSample * ReverbWetMixRatio;

		// 6. Dynamic Resonant Low-Pass Filter (Cutoff sweeps with hold modulation, convergence energy, and overdrive)
		const float FilterCutoffFrequencyHz = FMath::Clamp(FilterBaselineCutoffHz + MaxHoldModulation * 2400.0f + CurrentConvergenceEnergy * 5500.0f + OverdriveIntensity * 4500.0f, 150.0f, 18000.0f);
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
