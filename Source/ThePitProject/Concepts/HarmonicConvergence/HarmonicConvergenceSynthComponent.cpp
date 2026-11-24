#include "HarmonicConvergenceSynthComponent.h"

float FConvergenceVoiceDSP::GenerateSample(float SampleRate, float EffectiveFreqHz, float& OutLeftSample, float& OutRightSample)
{
	if (!bIsVoiceActive && AmplitudeEnvelope <= 0.0005f && TransientEnvelope <= 0.0005f && IgnitionEnvelope <= 0.0005f)
	{
		OutLeftSample = 0.0f;
		OutRightSample = 0.0f;
		return 0.0f;
	}

	const float InvSampleRate = 1.0f / SampleRate;
	AmplitudeEnvelope = FMath::FInterpTo(AmplitudeEnvelope, bIsVoiceActive ? 1.0f : 0.0f, InvSampleRate, bIsVoiceActive ? 16.0f : 12.0f);

	// Harmonic Overtone Capacitor Charge: keeps fundamental locked in tune with the active chord while sweeping harmonic overtones (2f, 3f, 4f)
	const float ChargeRatio = FMath::Clamp(HoldModulationIntensity, 0.0f, 1.0f);
	const float HarmonicDrive = FMath::Pow(ChargeRatio, 1.35f);

	// Electric flutter modulation (14 Hz plasma shimmer)
	FlutterPhaseAngle += (14.0f * 2.0f * PI) * InvSampleRate;
	if (FlutterPhaseAngle > 2.0f * PI) FlutterPhaseAngle -= 2.0f * PI;
	const float ElectricFlutter = 1.0f + 0.15f * FMath::Sin(FlutterPhaseAngle);

	CarrierPhaseAngle += (EffectiveFreqHz * 2.0f * PI) * InvSampleRate;
	if (CarrierPhaseAngle > 2.0f * PI) CarrierPhaseAngle -= 2.0f * PI;

	HarmonicPhaseAngle += (EffectiveFreqHz * 2.0f * 2.0f * PI) * InvSampleRate;
	if (HarmonicPhaseAngle > 2.0f * PI) HarmonicPhaseAngle -= 2.0f * PI;

	const float Fundamental = FMath::Sin(CarrierPhaseAngle);
	const float OctaveHarmonic = FMath::Sin(HarmonicPhaseAngle) * (0.25f + 0.35f * HarmonicDrive);
	const float FifthAndDoubleOctave = (FMath::Sin(CarrierPhaseAngle * 3.0f) * 0.22f + FMath::Sin(HarmonicPhaseAngle * 2.0f) * 0.18f) * HarmonicDrive;

	const float DuckingFactor = (ChargeRatio >= 0.99f) ? 0.15f : 1.0f;
	const float ChargingGain = (0.15f + 0.30f * ChargeRatio) * DuckingFactor;

	float MonoVoiceSample = (Fundamental * 0.65f + OctaveHarmonic + FifthAndDoubleOctave) * ElectricFlutter * ChargingGain * AmplitudeEnvelope;

	// Tactile Note-On Drum Pad Bite (~70ms crisp percussive transient)
	if (TransientEnvelope > 0.001f)
	{
		TransientEnvelope = FMath::FInterpTo(TransientEnvelope, 0.0f, InvSampleRate, 22.0f);
		TransientPhaseAngle += (EffectiveFreqHz * 2.0f * 2.0f * PI) * InvSampleRate;
		if (TransientPhaseAngle > 2.0f * PI) TransientPhaseAngle -= 2.0f * PI;

		const float TactileBite = (FMath::Sin(TransientPhaseAngle) * 0.65f + FMath::Sin(TransientPhaseAngle * 2.0f) * 0.35f) * TransientEnvelope * 0.50f;
		MonoVoiceSample += TactileBite;
	}

	// Crystal Ignition Flash Strike (~250ms glass chime on 100% full charge)
	if (IgnitionEnvelope > 0.001f)
	{
		IgnitionEnvelope = FMath::FInterpTo(IgnitionEnvelope, 0.0f, InvSampleRate, 6.0f);
		IgnitionPhaseAngle += (EffectiveFreqHz * 2.0f * 2.0f * PI) * InvSampleRate;
		if (IgnitionPhaseAngle > 2.0f * PI) IgnitionPhaseAngle -= 2.0f * PI;

		const float IgnitionChime = (FMath::Sin(IgnitionPhaseAngle) * 0.7f + FMath::Sin(IgnitionPhaseAngle * 3.0f) * 0.3f) * IgnitionEnvelope * 0.55f;
		MonoVoiceSample += IgnitionChime;
	}

	const float PanAngle = (StereoPanPosition + 1.0f) * 0.25f * PI;
	OutLeftSample = MonoVoiceSample * FMath::Cos(PanAngle);
	OutRightSample = MonoVoiceSample * FMath::Sin(PanAngle);

	return MonoVoiceSample;
}

float FRhythmicPulseDSP::GenerateSample(float SampleRate, float EffectiveFreqHz, float& OutLeftSample, float& OutRightSample)
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
			if (SlotIndex == 0 || SlotIndex == 3 || OctaveOffset == 0)
			{
				// Punchy analog kick with transient click
				Envelope = FMath::FInterpTo(Envelope, 0.0f, InvSampleRate, 6.0f);
				PitchDecay = FMath::FInterpTo(PitchDecay, 0.0f, InvSampleRate, 28.0f);

				const float KickFreqHz = 48.0f + 95.0f * PitchDecay;
				PhaseAngle += (KickFreqHz * 2.0f * PI) * InvSampleRate;
				if (PhaseAngle > 2.0f * PI) PhaseAngle -= 2.0f * PI;

				const float Click = FMath::FRandRange(-1.0f, 1.0f) * PitchDecay * 0.25f;
				const float Body = FMath::Tanh((FMath::Sin(PhaseAngle) + FMath::Sin(PhaseAngle * 2.0f) * 0.18f) * 1.35f);
				MonoSample = (Body * Envelope + Click) * SlotGain * 0.55f;
			}
			else if (SlotIndex % 4 == 2)
			{
				// Tight electronic snare / rim groove
				Envelope = FMath::FInterpTo(Envelope, 0.0f, InvSampleRate, 11.0f);
				PitchDecay = FMath::FInterpTo(PitchDecay, 0.0f, InvSampleRate, 32.0f);

				const float SnareFreqHz = 165.0f + 120.0f * PitchDecay;
				PhaseAngle += (SnareFreqHz * 2.0f * PI) * InvSampleRate;
				if (PhaseAngle > 2.0f * PI) PhaseAngle -= 2.0f * PI;

				const float Body = FMath::Sin(PhaseAngle) * PitchDecay * 0.5f;
				const float Snap = FMath::FRandRange(-1.0f, 1.0f) * Envelope * 0.65f;
				MonoSample = (Body + Snap) * SlotGain * 0.38f;
			}
			else
			{
				// Pitched FM bassline (octave-folded into 55..175 Hz so every station has a distinct bass note)
				Envelope = FMath::FInterpTo(Envelope, 0.0f, InvSampleRate, 4.5f);
				PitchDecay = FMath::FInterpTo(PitchDecay, 0.0f, InvSampleRate, 14.0f);

				float BassFreq = FMath::Max(EffectiveFreqHz, 20.0f);
				while (BassFreq > 175.0f) BassFreq *= 0.5f;
				while (BassFreq < 55.0f) BassFreq *= 2.0f;

				PhaseAngle += (BassFreq * 2.0f * PI) * InvSampleRate;
				if (PhaseAngle > 2.0f * PI) PhaseAngle -= 2.0f * PI;
				PhaseAngle2 += (BassFreq * 2.0f * 2.0f * PI) * InvSampleRate;
				if (PhaseAngle2 > 2.0f * PI) PhaseAngle2 -= 2.0f * PI;

				const float FmIndex = 0.4f + 1.4f * PitchDecay;
				const float BassWave = FMath::Tanh(FMath::Sin(PhaseAngle + FmIndex * FMath::Sin(PhaseAngle2)) * 1.3f);
				MonoSample = BassWave * Envelope * SlotGain * 0.42f;
			}
		}
		break;

	case EVoiceTimbreProfile::CrystallineChime:
		{
			if (OctaveOffset >= 3 && (SlotIndex % 2 == 1))
			{
				// Crisp shaker / hi-hat transient + glassy ping
				Envelope = FMath::FInterpTo(Envelope, 0.0f, InvSampleRate, 18.0f);
				PhaseAngle += (EffectiveFreqHz * 2.0f * PI) * InvSampleRate;
				if (PhaseAngle > 2.0f * PI) PhaseAngle -= 2.0f * PI;

				const float HatNoise = FMath::FRandRange(-1.0f, 1.0f) * 0.65f;
				const float Ping = FMath::Sin(PhaseAngle) * 0.35f;
				MonoSample = (HatNoise + Ping) * Envelope * SlotGain * 0.18f;
			}
			else
			{
				// Shimmering FM glass marimba / arp
				Envelope = FMath::FInterpTo(Envelope, 0.0f, InvSampleRate, 6.5f);
				PitchDecay = FMath::FInterpTo(PitchDecay, 0.0f, InvSampleRate, 16.0f);

				PhaseAngle += (EffectiveFreqHz * 2.0f * PI) * InvSampleRate;
				if (PhaseAngle > 2.0f * PI) PhaseAngle -= 2.0f * PI;
				PhaseAngle2 += (EffectiveFreqHz * 3.0f * 2.0f * PI) * InvSampleRate;
				if (PhaseAngle2 > 2.0f * PI) PhaseAngle2 -= 2.0f * PI;

				const float Mod = (0.25f + 0.85f * PitchDecay) * FMath::Sin(PhaseAngle2);
				const float ChimeSample = FMath::Sin(PhaseAngle + Mod) * 0.65f + FMath::Sin(PhaseAngle * 2.0f) * 0.25f + FMath::Sin(PhaseAngle * 4.0f) * 0.10f;
				MonoSample = ChimeSample * Envelope * SlotGain * 0.22f;
			}
		}
		break;

	case EVoiceTimbreProfile::VortexSweep:
		{
			// Rising/falling FM harmonic sweep
			Envelope = FMath::FInterpTo(Envelope, 0.0f, InvSampleRate, 3.2f);
			PitchDecay = FMath::FInterpTo(PitchDecay, 0.0f, InvSampleRate, 4.5f);

			PhaseAngle += (EffectiveFreqHz * 2.0f * PI) * InvSampleRate;
			if (PhaseAngle > 2.0f * PI) PhaseAngle -= 2.0f * PI;
			PhaseAngle2 += (EffectiveFreqHz * 1.5f * 2.0f * PI) * InvSampleRate;
			if (PhaseAngle2 > 2.0f * PI) PhaseAngle2 -= 2.0f * PI;

			const float SweepIndex = (1.0f - PitchDecay) * 1.6f + 0.25f;
			const float Swirl = FMath::Sin(PhaseAngle + SweepIndex * FMath::Sin(PhaseAngle2)) * 0.7f + FMath::Sin(PhaseAngle * 2.0f) * 0.3f;
			MonoSample = Swirl * Envelope * SlotGain * 0.22f;
		}
		break;

	case EVoiceTimbreProfile::WarmPad:
	default:
		{
			// Sustained detuned single chord-voice pad (phase-continuous across chord voice-leading)
			Envelope = FMath::FInterpTo(Envelope, 0.0f, InvSampleRate, 1.8f);

			PhaseAngle += (EffectiveFreqHz * 2.0f * PI) * InvSampleRate;
			if (PhaseAngle > 2.0f * PI) PhaseAngle -= 2.0f * PI;
			PhaseAngle2 += (EffectiveFreqHz * 1.006f * 2.0f * PI) * InvSampleRate;
			if (PhaseAngle2 > 2.0f * PI) PhaseAngle2 -= 2.0f * PI;

			const float Fundamental = (FMath::Sin(PhaseAngle) + FMath::Sin(PhaseAngle2)) * 0.42f;
			const float Harmonic2 = FMath::Sin(PhaseAngle * 2.0f) * 0.16f;
			MonoSample = FMath::Tanh((Fundamental + Harmonic2) * 1.1f) * Envelope * SlotGain * 0.24f;
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

	MarkovTransitions = {
		{ TEXT("I (1)"),   {  0.0f, 50.0f, 33.0f, 66.0f, 66.0f, 33.0f, 10.0f } },
		{ TEXT("II (2)"),  { 25.0f,  0.0f, 20.0f, 40.0f, 80.0f, 20.0f, 15.0f } },
		{ TEXT("III (3)"), { 25.0f, 35.0f,  0.0f, 66.0f, 40.0f, 66.0f, 10.0f } },
		{ TEXT("IV (4)"),  { 60.0f, 50.0f, 20.0f,  0.0f, 75.0f, 33.0f, 10.0f } },
		{ TEXT("V (5)"),   { 80.0f, 25.0f, 25.0f, 50.0f,  0.0f, 50.0f, 10.0f } },
		{ TEXT("VI (6)"),  { 30.0f, 60.0f, 33.0f, 70.0f, 55.0f,  0.0f, 10.0f } },
		{ TEXT("VII (7)"), { 85.0f, 20.0f, 33.0f, 25.0f, 45.0f, 25.0f,  0.0f } }
	};

	ScaleModeTransitions = {
		{ TEXT("Lydian Celestial"), { 35.0f, 25.0f, 30.0f, 10.0f } },
		{ TEXT("Dorian Ambient"),   { 30.0f, 35.0f, 15.0f, 20.0f } },
		{ TEXT("Ionian Major"),     { 35.0f, 20.0f, 35.0f, 10.0f } },
		{ TEXT("Aeolian Minor"),    { 20.0f, 40.0f, 10.0f, 30.0f } }
	};
	InitializeVoicing_Unlocked();
}

bool UHarmonicConvergenceSynthComponent::Init(int32& SampleRate)
{
	NumChannels = 2;
	CurrentSampleRate = SampleRate > 0 ? static_cast<float>(SampleRate) : 48000.0f;

	LeftChannelFilter.Reset();
	RightChannelFilter.Reset();
	DiffusionTank.Reset();

	{
		FScopeLock Lock(&AudioLock);
		CurrentMeasureIndex = 0;
		CurrentScaleDegree = 0;
		InitializeVoicing_Unlocked();
	}

	return true;
}

void UHarmonicConvergenceSynthComponent::InitializeVoicing_Unlocked()
{
	if (ActiveScaleMode != EHarmonicScaleMode::CustomFrequencies)
	{
		ApplyChordVoicing_Unlocked(FHarmonicChord::FromScaleDegree(ActiveScaleMode, CurrentScaleDegree, 0), true);
	}
	else
	{
		CurrentChordRootRatio = 1.0f;
		CurrentVoicedSemitones[0] = 0;
		CurrentVoicedSemitones[1] = 3;
		CurrentVoicedSemitones[2] = 7;
		CurrentChordInversion = 0;
		for (int32 i = 0; i < 3; ++i)
		{
			const float Ratio = FHarmonicChord::SemitonesToRatio(static_cast<float>(CurrentVoicedSemitones[i]));
			TargetVoiceRatios[i] = Ratio;
			CurrentVoiceRatios[i] = Ratio;
		}
	}
}

void UHarmonicConvergenceSynthComponent::ApplyChordVoicing_Unlocked(const FHarmonicChord& NextChord, bool bSnapImmediate)
{
	int32 NextVoices[3];
	int32 NextInversion = 0;

	if (bSnapImmediate || !bEnableAutoVoiceLeading)
	{
		NextInversion = FMath::Clamp(NextChord.Inversion, 0, 2);
		NextChord.GetCloseVoicing(NextVoices, NextInversion);
	}
	else
	{
		NextChord.ChooseBestVoiceLeading(CurrentVoicedSemitones, NextVoices, NextInversion);
	}

	CurrentChordInversion = NextInversion;
	CurrentChordRootRatio = FHarmonicChord::SemitonesToRatio(static_cast<float>(NextChord.RootSemitoneOffset));
	for (int32 i = 0; i < 3; ++i)
	{
		CurrentVoicedSemitones[i] = NextVoices[i];
		const float Ratio = FHarmonicChord::SemitonesToRatio(static_cast<float>(NextVoices[i]));
		TargetVoiceRatios[i] = Ratio;
		if (bSnapImmediate)
		{
			CurrentVoiceRatios[i] = Ratio;
		}
	}
}

void UHarmonicConvergenceSynthComponent::AdvanceChordStep_Unlocked()
{
	if (ActiveScaleMode != EHarmonicScaleMode::CustomFrequencies)
	{
		const int32 ModeIdx = FMath::Clamp(static_cast<int32>(ActiveScaleMode), 0, 3);
		if (ScaleModeTransitions.IsValidIndex(ModeIdx))
		{
			ActiveScaleMode = ScaleModeTransitions[ModeIdx].SampleNextMode(ScaleModeDynamicBias, FMath::FRand(), ActiveScaleMode);
		}

		const int32 FallbackDegree = (CurrentScaleDegree + 1) % 7;
		CurrentScaleDegree = MarkovTransitions.IsValidIndex(CurrentScaleDegree)
			? MarkovTransitions[CurrentScaleDegree].SampleNextDegree(ChordDegreeDynamicBias, FMath::FRand(), FallbackDegree)
			: FallbackDegree;

		FHarmonicChord NextChord = FHarmonicChord::FromScaleDegree(ActiveScaleMode, CurrentScaleDegree, 0);
		if (NextChord.Quality == EHarmonicChordQuality::Diminished)
		{
			// Substitute the consonant diatonic triad a 3rd below (shares 2 common tones, avoids tritone dissonance)
			NextChord = FHarmonicChord::FromScaleDegree(ActiveScaleMode, (CurrentScaleDegree + 5) % 7, 1);
		}
		ApplyChordVoicing_Unlocked(NextChord, false);
	}
}

void UHarmonicConvergenceSynthComponent::SetScaleMode(EHarmonicScaleMode NewScaleMode)
{
	FScopeLock Lock(&AudioLock);
	ActiveScaleMode = NewScaleMode;
	if (ActiveScaleMode != EHarmonicScaleMode::CustomFrequencies)
	{
		ApplyChordVoicing_Unlocked(FHarmonicChord::FromScaleDegree(ActiveScaleMode, CurrentScaleDegree, 0), false);
	}
}

EHarmonicScaleMode UHarmonicConvergenceSynthComponent::GetActiveScaleMode() const
{
	FScopeLock Lock(&AudioLock);
	return ActiveScaleMode;
}

void UHarmonicConvergenceSynthComponent::SetScaleModeBiasWeights(float LydianBias, float DorianBias, float IonianBias, float AeolianBias)
{
	FScopeLock Lock(&AudioLock);
	ScaleModeDynamicBias[0] = FMath::Max(0.01f, LydianBias);
	ScaleModeDynamicBias[1] = FMath::Max(0.01f, DorianBias);
	ScaleModeDynamicBias[2] = FMath::Max(0.01f, IonianBias);
	ScaleModeDynamicBias[3] = FMath::Max(0.01f, AeolianBias);
}

void UHarmonicConvergenceSynthComponent::SetChordTensionBias(float Energy01, bool bOverdrive)
{
	FScopeLock Lock(&AudioLock);
	const float T = FMath::Clamp(Energy01, 0.0f, 1.0f);
	const float OverdriveBoost = bOverdrive ? 0.35f : 0.0f;

	// Low energy favors stable I / IV / V; high energy & overdrive unlock II / III / VI / VII
	ChordDegreeDynamicBias[0] = FMath::Lerp(1.45f, 0.90f, T); // I
	ChordDegreeDynamicBias[1] = FMath::Lerp(0.65f, 1.50f, T) + OverdriveBoost; // II
	ChordDegreeDynamicBias[2] = FMath::Lerp(0.60f, 1.45f, T) + OverdriveBoost; // III
	ChordDegreeDynamicBias[3] = FMath::Lerp(1.30f, 1.05f, T); // IV
	ChordDegreeDynamicBias[4] = FMath::Lerp(1.35f, 1.15f, T); // V
	ChordDegreeDynamicBias[5] = FMath::Lerp(0.85f, 1.45f, T) + OverdriveBoost; // VI
	ChordDegreeDynamicBias[6] = FMath::Lerp(0.45f, 1.55f, T) + OverdriveBoost; // VII
}

int32 UHarmonicConvergenceSynthComponent::GetCurrentScaleDegree() const
{
	FScopeLock Lock(&AudioLock);
	return CurrentScaleDegree;
}

float UHarmonicConvergenceSynthComponent::ResolveVoiceFrequencyHz_Unlocked(EChordVoiceRole VoiceRole, int32 OctaveOffset, float FallbackFrequencyHz, bool bUseTargetRatio) const
{
	if (VoiceRole == EChordVoiceRole::FixedPitch)
	{
		return FallbackFrequencyHz;
	}

	const int32 VoiceIdx = FMath::Clamp(static_cast<int32>(VoiceRole), 0, 2);
	const float OctaveMult = static_cast<float>(1 << FMath::Clamp(OctaveOffset, 0, 4));
	const float Ratio = bUseTargetRatio ? TargetVoiceRatios[VoiceIdx] : CurrentVoiceRatios[VoiceIdx];
	return RootFrequencyHz * OctaveMult * Ratio;
}

int32 UHarmonicConvergenceSynthComponent::GetCurrentChordInversion() const
{
	FScopeLock Lock(&AudioLock);
	return CurrentChordInversion;
}

float UHarmonicConvergenceSynthComponent::GetChordVoiceFrequencyHz(EChordVoiceRole VoiceRole, int32 OctaveOffset, float FallbackFrequencyHz) const
{
	FScopeLock Lock(&AudioLock);
	return ResolveVoiceFrequencyHz_Unlocked(VoiceRole, OctaveOffset, FallbackFrequencyHz, true);
}

void UHarmonicConvergenceSynthComponent::NoteOn(int32 VoiceIndex, float NoteFrequencyHz, float StereoPan, EChordVoiceRole VoiceRole, int32 OctaveOffset)
{
	FScopeLock Lock(&AudioLock);
	if (VoiceIndex >= 0 && VoiceIndex < MaxVoices)
	{
		Voices[VoiceIndex].NoteFrequencyHz = NoteFrequencyHz;
		Voices[VoiceIndex].VoiceRole = VoiceRole;
		Voices[VoiceIndex].OctaveOffset = FMath::Clamp(OctaveOffset, 0, 4);
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
		RhythmicSlots[VoiceIndex].Trigger();
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

void UHarmonicConvergenceSynthComponent::TriggerEchoRipple(float ChimeFrequencyHz, float StereoPan, int32 VoiceIndex)
{
	FScopeLock Lock(&AudioLock);
	const int32 SafeVoiceIndex = FMath::Clamp(VoiceIndex, 0, MaxVoices - 1);
	Voices[SafeVoiceIndex].ExpiryFrequencyHz = FMath::Max(20.0f, ChimeFrequencyHz);
	Voices[SafeVoiceIndex].StereoPanPosition = FMath::Clamp(StereoPan, -1.0f, 1.0f);
	Voices[SafeVoiceIndex].TriggerExpiry();
}

void UHarmonicConvergenceSynthComponent::SetRhythmicPulseSlot(int32 SlotIndex, bool bActive, int32 RhythmicMask, float NoteFrequencyHz, float StereoPan, EVoiceTimbreProfile TimbreProfile, float Gain, EChordVoiceRole VoiceRole, int32 OctaveOffset)
{
	FScopeLock Lock(&AudioLock);
	if (SlotIndex >= 0 && SlotIndex < MaxRhythmicSlots)
	{
		RhythmicSlots[SlotIndex].SlotIndex = SlotIndex;
		RhythmicSlots[SlotIndex].bIsActive = bActive;
		RhythmicSlots[SlotIndex].RhythmicPatternMask = RhythmicMask;
		RhythmicSlots[SlotIndex].BaseFrequencyHz = NoteFrequencyHz;
		RhythmicSlots[SlotIndex].VoiceRole = VoiceRole;
		RhythmicSlots[SlotIndex].OctaveOffset = FMath::Clamp(OctaveOffset, 0, 4);
		RhythmicSlots[SlotIndex].StereoPanPosition = FMath::Clamp(StereoPan, -1.0f, 1.0f);
		RhythmicSlots[SlotIndex].TimbreProfile = TimbreProfile;
		RhythmicSlots[SlotIndex].SlotGain = FMath::Clamp(Gain, 0.0f, 1.0f);
	}
}

void UHarmonicConvergenceSynthComponent::SetRhythmicSlotGain(int32 SlotIndex, float Gain)
{
	FScopeLock Lock(&AudioLock);
	if (SlotIndex >= 0 && SlotIndex < MaxRhythmicSlots)
	{
		RhythmicSlots[SlotIndex].SlotGain = FMath::Clamp(Gain, 0.0f, 1.0f);
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

	int32 ActiveRhythmicSlotCount = 0;
	for (int32 SlotIndex = 0; SlotIndex < MaxRhythmicSlots; ++SlotIndex)
	{
		if (RhythmicSlots[SlotIndex].bIsActive || RhythmicSlots[SlotIndex].Envelope > 0.0005f)
		{
			ActiveRhythmicSlotCount++;
		}
	}
	const float DynamicRhythmicHeadroom = (ActiveRhythmicSlotCount > 0) ? (1.0f / FMath::Pow(static_cast<float>(ActiveRhythmicSlotCount), 0.65f)) : 1.0f;

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
					AdvanceChordStep_Unlocked();
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

		for (int32 i = 0; i < 3; ++i)
		{
			CurrentVoiceRatios[i] = FMath::FInterpTo(CurrentVoiceRatios[i], TargetVoiceRatios[i], InvSampleRate, 8.0f);
		}

		float AccumulatedLeftChannel = 0.0f;
		float AccumulatedRightChannel = 0.0f;
		float DirectSubLeftChannel = 0.0f;
		float DirectSubRightChannel = 0.0f;

		// 1. Synthesize Polyphonic Voices
		for (int32 VoiceIndex = 0; VoiceIndex < MaxVoices; ++VoiceIndex)
		{
			float VoiceLeftSample = 0.0f;
			float VoiceRightSample = 0.0f;
			const float VoiceFreqHz = ResolveVoiceFrequencyHz_Unlocked(
				Voices[VoiceIndex].VoiceRole,
				Voices[VoiceIndex].OctaveOffset,
				Voices[VoiceIndex].NoteFrequencyHz);
			Voices[VoiceIndex].GenerateSample(CurrentSampleRate, VoiceFreqHz, VoiceLeftSample, VoiceRightSample);
			AccumulatedLeftChannel += VoiceLeftSample * DynamicVoiceHeadroomScaling * 0.65f;
			AccumulatedRightChannel += VoiceRightSample * DynamicVoiceHeadroomScaling * 0.65f;
		}

		// 2. Central Grounding Sub-Bass Drone (tracks active chord's true harmonic root, octave-folded into 32..90 Hz) + Overdrive Sub-Octave
		float DroneFreqHz = RootFrequencyHz * CurrentChordRootRatio;
		while (DroneFreqHz > 90.0f) DroneFreqHz *= 0.5f;
		while (DroneFreqHz < 32.7f) DroneFreqHz *= 2.0f;

		// Wrap at 4*PI so both fundamental Sin(Phase) and sub-octave Sin(0.5*Phase) complete full seamless cycles without rectification/DC offset
		RootSubBassDronePhaseAngle += (DroneFreqHz * 2.0f * PI) * InvSampleRate;
		if (RootSubBassDronePhaseAngle > 4.0f * PI) RootSubBassDronePhaseAngle -= 4.0f * PI;

		const float DropDuck = 1.0f - OverdriveDropEnvelope * 0.7f;
		const float SubBassDroneSample = FMath::Sin(RootSubBassDronePhaseAngle) * (CurrentConvergenceEnergy * SubBassDroneVolume * DropDuck);
		const float OverdriveSubSample = FMath::Sin(RootSubBassDronePhaseAngle * 0.5f) * (OverdriveIntensity * 0.22f * DropDuck);
		DirectSubLeftChannel += (SubBassDroneSample + OverdriveSubSample);
		DirectSubRightChannel += (SubBassDroneSample + OverdriveSubSample);

		// 3. Persistent Rhythmic Beat Slots
		for (int32 SlotIndex = 0; SlotIndex < MaxRhythmicSlots; ++SlotIndex)
		{
			if (RhythmicSlots[SlotIndex].Envelope > 0.0005f)
			{
				float PulseLeft = 0.0f;
				float PulseRight = 0.0f;
				const bool bPluckedOrPercussive = (RhythmicSlots[SlotIndex].TimbreProfile == EVoiceTimbreProfile::CrystallineChime ||
				                                   RhythmicSlots[SlotIndex].TimbreProfile == EVoiceTimbreProfile::SubBassPad);
				const float SlotFreqHz = ResolveVoiceFrequencyHz_Unlocked(
					RhythmicSlots[SlotIndex].VoiceRole,
					RhythmicSlots[SlotIndex].OctaveOffset,
					RhythmicSlots[SlotIndex].BaseFrequencyHz,
					bPluckedOrPercussive);
				RhythmicSlots[SlotIndex].GenerateSample(CurrentSampleRate, SlotFreqHz, PulseLeft, PulseRight);
				if (RhythmicSlots[SlotIndex].TimbreProfile == EVoiceTimbreProfile::SubBassPad)
				{
					DirectSubLeftChannel += PulseLeft * DynamicRhythmicHeadroom;
					DirectSubRightChannel += PulseRight * DynamicRhythmicHeadroom;
				}
				else
				{
					AccumulatedLeftChannel += PulseLeft * DynamicRhythmicHeadroom;
					AccumulatedRightChannel += PulseRight * DynamicRhythmicHeadroom;
				}
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
			DirectSubLeftChannel += DropSample;
			DirectSubRightChannel += DropSample;
		}

		// Echo Ripple Crystalline Bell on crystal expiration (polyphonic per-voice + fallback global, post-filter sparkle fed into reverb)
		float SparkleLeftChannel = 0.0f;
		float SparkleRightChannel = 0.0f;
		for (int32 VoiceIndex = 0; VoiceIndex < MaxVoices; ++VoiceIndex)
		{
			FConvergenceVoiceDSP& V = Voices[VoiceIndex];
			if (V.ExpiryEnvelope > 0.001f)
			{
				V.ExpiryEnvelope = FMath::FInterpTo(V.ExpiryEnvelope, 0.0f, InvSampleRate, 3.5f);
				float BellFreqHz = FMath::Max(20.0f, V.ExpiryFrequencyHz);
				while (BellFreqHz < 523.25f) BellFreqHz *= 2.0f;
				while (BellFreqHz > 2093.0f) BellFreqHz *= 0.5f;

				// Slightly stagger decay/pitch per station index (+0.3% per unit) so simultaneous expiries shimmer instead of phase-canceling
				BellFreqHz *= (1.0f + (VoiceIndex % 4) * 0.002f);
				V.ExpiryPhaseAngle += (BellFreqHz * 2.0f * PI) * InvSampleRate;
				if (V.ExpiryPhaseAngle > 4.0f * PI) V.ExpiryPhaseAngle -= 4.0f * PI;

				// Descending 2-tone crystalline release motif (high 2f harmonic fades into 1f fundamental + 0.5f sub-bell)
				const float HighHarmonic = FMath::Sin(V.ExpiryPhaseAngle * 2.0f) * (0.35f * V.ExpiryEnvelope);
				const float FundamentalBell = FMath::Sin(V.ExpiryPhaseAngle) * 0.55f + FMath::Sin(V.ExpiryPhaseAngle * 0.5f) * 0.20f;
				const float RippleSample = (FundamentalBell + HighHarmonic) * V.ExpiryEnvelope * 0.30f;
				const float RipplePanAngle = (V.StereoPanPosition + 1.0f) * 0.25f * PI;
				SparkleLeftChannel += RippleSample * FMath::Cos(RipplePanAngle);
				SparkleRightChannel += RippleSample * FMath::Sin(RipplePanAngle);
			}
		}

		// 4. Ambient Attract Chime (post-filter so it stays bright at 0 ConvergenceEnergy)
		if (AttractChimeEnvelope > 0.001f)
		{
			AttractChimeEnvelope = FMath::FInterpTo(AttractChimeEnvelope, 0.0f, InvSampleRate, 0.65f);
			AttractChimePhaseAngle += (AttractChimeFrequencyHz * 2.0f * PI) * InvSampleRate;
			if (AttractChimePhaseAngle > 2.0f * PI) AttractChimePhaseAngle -= 2.0f * PI;

			const float ChimeSample = FMath::Sin(AttractChimePhaseAngle) * AttractChimeEnvelope * 0.25f;
			SparkleLeftChannel += ChimeSample;
			SparkleRightChannel += ChimeSample;
		}

		// 5. Ambient Stereo Diffusion Reverb Tank (fed only mid/high harmonic voices + chimes; sub-bass & kicks bypass reverb)
		float ReverbWetLeftSample = 0.0f;
		float ReverbWetRightSample = 0.0f;
		const float ReverbFeedbackGain = 0.45f + 0.18f * FMath::Clamp(CurrentConvergenceEnergy * 0.4f, 0.0f, 1.0f) + OverdriveIntensity * 0.08f;
		DiffusionTank.Process(AccumulatedLeftChannel + SparkleLeftChannel, AccumulatedRightChannel + SparkleRightChannel, ReverbFeedbackGain, ReverbWetLeftSample, ReverbWetRightSample);

		const float ReverbWetMixRatio = 0.18f + 0.14f * FMath::Clamp(CurrentConvergenceEnergy * 0.5f, 0.0f, 1.0f) + OverdriveIntensity * 0.06f;
		const float FilterInputLeft = DirectSubLeftChannel + AccumulatedLeftChannel * (1.0f - ReverbWetMixRatio * 0.5f) + ReverbWetLeftSample * ReverbWetMixRatio;
		const float FilterInputRight = DirectSubRightChannel + AccumulatedRightChannel * (1.0f - ReverbWetMixRatio * 0.5f) + ReverbWetRightSample * ReverbWetMixRatio;

		// 6. Dynamic Resonant Low-Pass Filter (opens smoothly without harsh high-end fizz; chimes summed after filter)
		const float FilterCutoffFrequencyHz = FMath::Clamp(FilterBaselineCutoffHz + MaxHoldModulation * 1800.0f + CurrentConvergenceEnergy * 2600.0f + OverdriveIntensity * 2500.0f, 150.0f, 12000.0f);
		const float FilteredOutputLeft = LeftChannelFilter.Process(FilterInputLeft, FilterCutoffFrequencyHz, FilterResonanceQ, CurrentSampleRate) + SparkleLeftChannel;
		const float FilteredOutputRight = RightChannelFilter.Process(FilterInputRight, FilterCutoffFrequencyHz, FilterResonanceQ, CurrentSampleRate) + SparkleRightChannel;

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

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHarmonicVoiceLeadingTest,
	"ThePitProject.HarmonicConvergence.VoiceLeading",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHarmonicVoiceLeadingTest::RunTest(const FString& Parameters)
{
	const FHarmonicChord FMajor{ 5, EHarmonicChordQuality::Major, 0 };
	const FHarmonicChord DMinor{ 2, EHarmonicChordQuality::Minor, 0 };
	int32 FMajorVoices[3], DMinorVoices[3], ChosenInversion = -1;
	FMajor.GetCloseVoicing(FMajorVoices, 0);
	DMinor.ChooseBestVoiceLeading(FMajorVoices, DMinorVoices, ChosenInversion);
	TestTrue(TEXT("F->Dm chooses 1st inversion [5, 9, 14]"),
		ChosenInversion == 1 && DMinorVoices[0] == 5 && DMinorVoices[1] == 9 && DMinorVoices[2] == 14);

	UHarmonicConvergenceSynthComponent* Synth = NewObject<UHarmonicConvergenceSynthComponent>();
	int32 SampleRate = 48000;
	Synth->Init(SampleRate);
	Synth->SetScaleMode(EHarmonicScaleMode::AeolianMinor);
	Synth->SetScaleModeBiasWeights(1000.0f, 0.0f, 0.0f, 0.0f);

	for (int32 Step = 0; Step < 32; ++Step)
	{
		Synth->AdvanceChordStep_Unlocked();
		const int32* Voicing = Synth->CurrentVoicedSemitones;
		TestTrue(TEXT("Bounded ascending close voicing"),
			Voicing[0] >= FHarmonicChord::MinVoiceSemitone &&
			Voicing[0] < Voicing[1] && Voicing[1] < Voicing[2] &&
			Voicing[2] < Voicing[0] + 12 && Voicing[2] <= FHarmonicChord::MaxVoiceSemitone);
	}
	TestEqual(TEXT("Markov scale bias transitions to Lydian"), Synth->GetActiveScaleMode(), EHarmonicScaleMode::LydianCelestial);

	Synth->SetConvergenceEnergy(1.5f);
	Synth->SetOverdriveIntensity(1.0f);
	float AudioBuffer[512] = {};
	Synth->OnGenerateAudio(AudioBuffer, 512);
	for (float Sample : AudioBuffer)
	{
		TestTrue(TEXT("Finite DSP sample"), FMath::IsFinite(Sample));
	}

	return true;
}
#endif
