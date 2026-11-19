#include "HarmonicConvergenceAudioManager.h"
#include "HarmonicConvergenceSynthComponent.h"
#include "InputManagerSubSystem.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "Quartz/AudioMixerClockHandle.h"
#include "Quartz/QuartzSubsystem.h"

AHarmonicConvergenceAudioManager::AHarmonicConvergenceAudioManager()
{
	PrimaryActorTick.bCanEverTick = true;

	ProceduralSynthComponent = CreateDefaultSubobject<UHarmonicConvergenceSynthComponent>(TEXT("ProceduralConvergenceSynth"));
	SetRootComponent(ProceduralSynthComponent);
}

AHarmonicStationActor* AHarmonicConvergenceAudioManager::FindStationByCrystal(const AHarmonicCrystalActor* Crystal) const
{
	if (!Crystal)
	{
		return nullptr;
	}
	for (AHarmonicStationActor* Station : Stations)
	{
		if (Station && Station->CrystalActor == Crystal)
		{
			return Station;
		}
	}
	return nullptr;
}

void AHarmonicConvergenceAudioManager::BeginPlay()
{
	if (UQuartzSubsystem* QuartzSubsystem = UQuartzSubsystem::Get(GetWorld()))
	{
		FQuartzClockSettings ClockSettings;
		ClockSettings.bIgnoreLevelChange = false;

		MasterQuartzClock = QuartzSubsystem->CreateNewClock(this, FName(TEXT("HarmonicMasterClock")), ClockSettings, true, true);
		if (MasterQuartzClock)
		{
			FQuartzQuantizationBoundary ImmediateBoundary;
			ImmediateBoundary.Quantization = EQuartzCommandQuantization::None;
			FOnQuartzCommandEventBP DummyDelegate;
			UQuartzClockHandle* ClockRef = MasterQuartzClock;
			MasterQuartzClock->SetBeatsPerMinute(this, ImmediateBoundary, DummyDelegate, ClockRef, MasterBPM);
			MasterQuartzClock->StartClock(this, ClockRef);
		}
	}

	TInlineComponentArray<UNiagaraComponent*> AttachedNiagaraComps(this);
	for (UNiagaraComponent* Comp : AttachedNiagaraComps)
	{
		if (Comp)
		{
			Comp->SetAutoActivate(false);
			Comp->DeactivateImmediate();
		}
	}

	DiscoverStations();
	ApplyHarmonicScaleFrequencies();

	for (AHarmonicStationActor* Station : Stations)
	{
		if (Station)
		{
			if (!Station->bUseCustomTapCutDuration)
			{
				Station->TapCutMaxDurationSeconds = TapCutMaxDurationSeconds;
			}
			if (!Station->bUseCustomSpawnRate)
			{
				Station->SpawnRate = StationBeamSpawnRate;
				Station->TrickleSpawnMultiplier = StationTrickleMultiplier;
				if (Station->BeamComponent)
				{
					Station->BeamComponent->SetVariableFloat(FName(TEXT("User.myVortexForce")), Station->SpawnRate * 1.5f);
				}
			}
			Station->InitializeAudioStem(MasterQuartzClock);
			Station->OnStationPressedChanged.AddDynamic(this, &AHarmonicConvergenceAudioManager::HandleStationPressedChanged);
			if (Station->CrystalActor)
			{
				if (!Station->CrystalActor->bUseCustomChargeTime)
				{
					Station->CrystalActor->ChargeThresholdSeconds = CrystalChargeTimeSeconds;
				}
				if (!Station->CrystalActor->bUseCustomActiveLifespan)
				{
					Station->CrystalActor->ActiveLifespanSeconds = CrystalActiveLifespanSeconds;
					Station->CrystalActor->RemainingLifespan = CrystalActiveLifespanSeconds;
				}
				Station->CrystalActor->OnCrystalActivatedEvent.AddDynamic(this, &AHarmonicConvergenceAudioManager::HandleCrystalActivated);
				Station->CrystalActor->OnCrystalExpiredEvent.AddDynamic(this, &AHarmonicConvergenceAudioManager::HandleCrystalExpired);
			}
		}
	}

	Super::BeginPlay();

	if (ProceduralSynthComponent)
	{
		ProceduralSynthComponent->SetMasterBPM(MasterBPM);
		if (!ProceduralSynthComponent->IsPlaying())
		{
			ProceduralSynthComponent->Start();
		}
	}

	SyncHardwareConfig();
}

void AHarmonicConvergenceAudioManager::DiscoverStations()
{
	Stations.RemoveAll([](const TObjectPtr<AHarmonicStationActor>& S) { return S == nullptr; });

	if (Stations.IsEmpty())
	{
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AHarmonicStationActor::StaticClass(), FoundActors);
		for (AActor* Actor : FoundActors)
		{
			if (AHarmonicStationActor* Station = Cast<AHarmonicStationActor>(Actor))
			{
				Stations.Add(Station);
			}
		}
	}

	for (AHarmonicStationActor* Station : Stations)
	{
		if (Station && (Station->ChannelName.IsNone() || Station->bAutoAssignAudioProfile))
		{
			Station->ApplyDefaultPresetForIndex();
		}
	}

	Stations.Sort([](const AHarmonicStationActor& A, const AHarmonicStationActor& B)
	{
		return A.StationIndex < B.StationIndex;
	});
}

void AHarmonicConvergenceAudioManager::SetMasterBPM(float NewBPM)
{
	MasterBPM = FMath::Clamp(NewBPM, 40.0f, 240.0f);
	if (MasterQuartzClock)
	{
		FQuartzQuantizationBoundary BarBoundary;
		BarBoundary.Quantization = EQuartzCommandQuantization::Bar;
		FOnQuartzCommandEventBP DummyDelegate;
		UQuartzClockHandle* ClockRef = MasterQuartzClock;
		MasterQuartzClock->SetBeatsPerMinute(this, BarBoundary, DummyDelegate, ClockRef, MasterBPM);
	}
	if (ProceduralSynthComponent)
	{
		ProceduralSynthComponent->SetMasterBPM(MasterBPM);
	}
	SyncHardwareConfig();
}

void AHarmonicConvergenceAudioManager::SyncHardwareConfig()
{
	UGameInstance* GI = GetGameInstance();
	UInputManagerSubSystem* InputSubsystem = GI ? GI->GetSubsystem<UInputManagerSubSystem>() : nullptr;
	if (!InputSubsystem)
	{
		return;
	}

	FPitSyncConfig Config;
	Config.ChargeTimeMs = FMath::Max(100, FMath::RoundToInt(CrystalChargeTimeSeconds * 1000.0f));
	Config.TapCutMaxMs = FMath::Max(50, FMath::RoundToInt(TapCutMaxDurationSeconds * 1000.0f));
	Config.IdleToAttractMs = FMath::Max(500, FMath::RoundToInt(AttractModeTimeoutSeconds * 1000.0f));
	Config.MaxActivePerStation = FMath::Clamp(MaxActiveCrystalsPerStation, 1, 4);
	Config.MasterBPM = MasterBPM;

	InputSubsystem->SetSyncConfigAndSync(Config);
}

void AHarmonicConvergenceAudioManager::SetHarmonicScale(EHarmonicScaleMode NewScaleMode)
{
	ScaleMode = NewScaleMode;
	ApplyHarmonicScaleFrequencies();
}

void AHarmonicConvergenceAudioManager::ApplyHarmonicScaleFrequencies()
{
	if (ScaleMode == EHarmonicScaleMode::CustomFrequencies)
	{
		return;
	}

	// 7-note scale intervals in semitones relative to root (0..11)
	static constexpr int32 ScaleLen = 7;
	static constexpr int32 ScaleIntervals[4][ScaleLen] = {
		{ 0, 2, 4, 6, 7, 9, 11 }, // LydianCelestial
		{ 0, 2, 3, 5, 7, 9, 10 }, // DorianAmbient
		{ 0, 2, 4, 5, 7, 9, 11 }, // IonianMajor
		{ 0, 2, 3, 5, 7, 8, 10 }  // AeolianMinor
	};

	int32 TableIdx = 0;
	switch (ScaleMode)
	{
	case EHarmonicScaleMode::DorianAmbient:   TableIdx = 1; break;
	case EHarmonicScaleMode::IonianMajor:     TableIdx = 2; break;
	case EHarmonicScaleMode::AeolianMinor:    TableIdx = 3; break;
	case EHarmonicScaleMode::LydianCelestial:
	default:                                  TableIdx = 0; break;
	}

	if (ProceduralSynthComponent)
	{
		ProceduralSynthComponent->SetRootFrequencyHz(RootNoteFrequencyHz);
	}

	// Distribute 24 stations across open voicings spanning ~4 octaves (octaves 0..4 relative to RootNoteFrequencyHz)
	static constexpr int32 StationOctaves[24] = {
		0, 1, 2, 3,   // Unit 1: Low foundation to mid-high
		1, 2, 3, 2,   // Unit 2: Bass & groove
		3, 1, 2, 4,   // Unit 3: Chimes & pads
		2, 2, 4, 1,   // Unit 4: Arps & harmonic pads
		3, 4, 2, 3,   // Unit 5: Leads & sweeps
		2, 3, 4, 1    // Unit 6: Shimmers & sub
	};

	for (AHarmonicStationActor* Station : Stations)
	{
		if (Station)
		{
			const int32 Idx = Station->StationIndex % 24;
			// Spread scale degrees across stations so neighbors form open fifths/thirds rather than clashing seconds
			const int32 Degree = (Idx * 2) % ScaleLen;
			const int32 TotalSemis = StationOctaves[Idx] * 12 + ScaleIntervals[TableIdx][Degree];
			Station->BaseFrequencyHz = RootNoteFrequencyHz * FHarmonicChord::SemitonesToRatio(static_cast<float>(TotalSemis));
		}
	}
}

void AHarmonicConvergenceAudioManager::HandleStationPressedChanged(AHarmonicStationActor* Station, bool bPressed)
{
	if (!Station)
	{
		return;
	}

	InactivityDurationSeconds = 0.0f;

	if (bPressed)
	{
		if (ProceduralSynthComponent)
		{
			ProceduralSynthComponent->NoteOn(Station->StationIndex, Station->BaseFrequencyHz, Station->PanPosition);
		}
		OnStationVoiceStarted.Broadcast(Station->ChannelName, Station->BaseFrequencyHz);
	}
	else
	{
		if (ProceduralSynthComponent)
		{
			ProceduralSynthComponent->NoteOff(Station->StationIndex);
		}
		OnStationVoiceStopped.Broadcast(Station->ChannelName);
	}
}

void AHarmonicConvergenceAudioManager::HandleCrystalActivated(AHarmonicCrystalActor* Crystal)
{
	AHarmonicStationActor* Station = FindStationByCrystal(Crystal);
	if (!Station)
	{
		return;
	}

	// Auto-Swap: If physical station unit exceeds MaxActiveCrystalsPerStation, deactivate the oldest active crystal
	const int32 UnitIdx = Station->StationIndex / 4;
	TArray<AHarmonicStationActor*> UnitActiveStations;

	for (AHarmonicStationActor* OtherStation : Stations)
	{
		if (OtherStation && OtherStation != Station && (OtherStation->StationIndex / 4) == UnitIdx)
		{
			if (OtherStation->IsCrystalActive())
			{
				UnitActiveStations.Add(OtherStation);
			}
		}
	}

	if (UnitActiveStations.Num() >= MaxActiveCrystalsPerStation)
	{
		// Find oldest active crystal in this unit (lowest remaining lifespan)
		AHarmonicStationActor* OldestStation = nullptr;
		float LowestRemainingLifespan = 999999.0f;

		for (AHarmonicStationActor* S : UnitActiveStations)
		{
			if (S->CrystalActor && S->CrystalActor->RemainingLifespan < LowestRemainingLifespan)
			{
				LowestRemainingLifespan = S->CrystalActor->RemainingLifespan;
				OldestStation = S;
			}
		}

		if (OldestStation && OldestStation->CrystalActor)
		{
			OldestStation->CrystalActor->DeactivateCrystal();
		}
	}

	if (ProceduralSynthComponent)
	{
		if (!Station->AudioStemAsset)
		{
			ProceduralSynthComponent->SetRhythmicPulseSlot(
				Station->StationIndex,
				true,
				Station->RhythmicPatternMask,
				Station->BaseFrequencyHz,
				Station->PanPosition,
				Station->TimbreProfile,
				1.0f);
		}
		ProceduralSynthComponent->TriggerVoiceIgnition(Station->StationIndex);
	}

	OnPersistentBeatActivated.Broadcast(Station->ChannelName, Station->RhythmicPatternMask, Station->BaseFrequencyHz);
}

void AHarmonicConvergenceAudioManager::HandleCrystalExpired(AHarmonicCrystalActor* Crystal)
{
	AHarmonicStationActor* Station = FindStationByCrystal(Crystal);
	if (!Station)
	{
		return;
	}

	if (ProceduralSynthComponent)
	{
		ProceduralSynthComponent->TriggerEchoRipple(Station->BaseFrequencyHz * 2.0f, Station->PanPosition);
		ProceduralSynthComponent->SetRhythmicPulseSlot(Station->StationIndex, false);
	}

	const int32 StationIdx = Stations.IndexOfByKey(Station);
	const int32 TotalStCount = Stations.Num();
	if (StationIdx != INDEX_NONE && TotalStCount > 1)
	{
		const int32 LeftNeighbor = (StationIdx + TotalStCount - 1) % TotalStCount;
		const int32 RightNeighbor = (StationIdx + 1) % TotalStCount;
		if (Stations[LeftNeighbor] && Stations[LeftNeighbor]->CrystalActor && !Stations[LeftNeighbor]->IsCrystalActive())
		{
			Stations[LeftNeighbor]->CrystalActor->TriggerBeatPulse(0.55f);
		}
		if (Stations[RightNeighbor] && Stations[RightNeighbor]->CrystalActor && !Stations[RightNeighbor]->IsCrystalActive())
		{
			Stations[RightNeighbor]->CrystalActor->TriggerBeatPulse(0.55f);
		}
	}

	OnCrystalEchoRippleTriggered.Broadcast(Station->StationIndex, Station->GetCrystalWorldLocation());
	OnPersistentBeatDeactivated.Broadcast(Station->StationIndex);

	// Notify physical station hardware that crystal has expired (clears locked LED state)
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UInputManagerSubSystem* InputSubsystem = GI->GetSubsystem<UInputManagerSubSystem>())
		{
			InputSubsystem->SendLEDState(Station->ChannelName, 0.0f);
		}
	}
}

void AHarmonicConvergenceAudioManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateGameAndSound(DeltaTime);

	// 16-Step Beat Synchronization: pulse active crystals whose station pattern matches the current 16th-note step
	if (ProceduralSynthComponent)
	{
		const int32 CurrentStep = ProceduralSynthComponent->GetCurrentBeatStep();
		if (CurrentStep != LastBeatStep)
		{
			LastBeatStep = CurrentStep;
			const int32 StepBit = (1 << CurrentStep);

			// Broadcast musical beat pulse to all physical station ESP32s on every quarter-note
			if (CurrentStep % 4 == 0)
			{
				if (UGameInstance* GI = GetGameInstance())
				{
					if (UInputManagerSubSystem* InputSubsystem = GI->GetSubsystem<UInputManagerSubSystem>())
					{
						InputSubsystem->SendOSCFloat(TEXT("/pit/beat"), 1.0f);
					}
				}
			}

			for (AHarmonicStationActor* Station : Stations)
			{
				if (Station && Station->IsCrystalActive() && (Station->RhythmicPatternMask & StepBit))
				{
					Station->CrystalActor->TriggerBeatPulse(1.0f);
					OnStationBeatPulse.Broadcast(Station->StationIndex, 1.0f, Station->StreamColor);
				}
			}
		}
	}
}

void AHarmonicConvergenceAudioManager::UpdateGameAndSound(float DeltaTime)
{
	ActiveVoiceCount = 0;
	int32 CurrentlyActiveLoopers = 0;
	float TotalModulationAccumulator = 0.0f;

	for (AHarmonicStationActor* Station : Stations)
	{
		if (Station && Station->IsCrystalActive())
		{
			CurrentlyActiveLoopers++;
		}
	}

	const bool bGlobalCanActivate = (CurrentlyActiveLoopers < MaxActiveCrystals);

	for (AHarmonicStationActor* Station : Stations)
	{
		if (!Station)
		{
			continue;
		}

		if (Station->CrystalActor)
		{
			// Global room ceiling governs whether any more crystals can activate
			Station->CrystalActor->bCanActivate = bGlobalCanActivate;
		}

		if (Station->bIsPressed)
		{
			ActiveVoiceCount++;
			const float Charge = Station->GetChargeProgress();
			TotalModulationAccumulator += Charge;
			if (ProceduralSynthComponent)
			{
				ProceduralSynthComponent->SetVoiceModulation(Station->StationIndex, Charge);
			}
		}

		if (Station->IsCrystalActive() && ProceduralSynthComponent && !Station->AudioStemAsset && Station->CrystalActor->RemainingLifespan < 6.0f)
		{
			const float AudioFadeAlpha = FMath::Clamp(Station->CrystalActor->RemainingLifespan / 6.0f, 0.0f, 1.0f);
			ProceduralSynthComponent->SetRhythmicPulseSlot(
				Station->StationIndex,
				true,
				Station->RhythmicPatternMask,
				Station->BaseFrequencyHz,
				Station->PanPosition,
				Station->TimbreProfile,
				AudioFadeAlpha);
		}
	}

	// Granular Convergence Energy: starts at BaseActiveIntensity (1.0) on first active crystal/button
	// and steps up by IntensityStepPerCrystal (0.25) per additional active source up to 2.0
	const int32 CombinedActiveSources = FMath::Max(ActiveVoiceCount, CurrentlyActiveLoopers);
	float TargetConvergenceEnergy = 0.0f;
	if (CombinedActiveSources > 0)
	{
		const float SteppedIntensity = BaseActiveIntensity + static_cast<float>(CombinedActiveSources - 1) * IntensityStepPerCrystal;
		const float HoldBoost = (ActiveVoiceCount > 0) ? FMath::Min(0.25f, TotalModulationAccumulator * 0.1f) : 0.0f;
		TargetConvergenceEnergy = FMath::Clamp(SteppedIntensity + HoldBoost, 0.0f, 2.0f);
	}
	ConvergenceEnergy = FMath::FInterpTo(ConvergenceEnergy, TargetConvergenceEnergy, DeltaTime, 5.0f);

	const float CoreBrightness = FMath::Pow(ConvergenceEnergy, 1.5f);
	for (AHarmonicStationActor* Station : Stations)
	{
		if (Station)
		{
			Station->UpdateBeamAndStem(DeltaTime, ConvergenceEnergy, ActiveVoiceCount, CoreBrightness);
		}
	}

	if (ProceduralSynthComponent)
	{
		ProceduralSynthComponent->SetConvergenceEnergy(ConvergenceEnergy);
	}

	// Harmonic Overdrive (Collective Drop)
	const bool bShouldOverdrive = (CurrentlyActiveLoopers >= OverdriveThreshold);
	if (bShouldOverdrive != bIsHarmonicOverdrive)
	{
		bIsHarmonicOverdrive = bShouldOverdrive;
		if (bIsHarmonicOverdrive && ProceduralSynthComponent)
		{
			ProceduralSynthComponent->TriggerOverdriveDrop();
		}
		OnHarmonicOverdriveStateChanged.Broadcast(bIsHarmonicOverdrive, OverdriveIntensity);
	}

	const float TargetOverdrive = bIsHarmonicOverdrive ? FMath::Clamp((CurrentlyActiveLoopers - OverdriveThreshold + 1) / 4.0f, 0.45f, 1.0f) : 0.0f;
	OverdriveIntensity = FMath::FInterpTo(OverdriveIntensity, TargetOverdrive, DeltaTime, 3.0f);

	if (ProceduralSynthComponent)
	{
		ProceduralSynthComponent->SetOverdriveIntensity(OverdriveIntensity);
	}

	UpdateAttractMode(DeltaTime, CurrentlyActiveLoopers);
}

void AHarmonicConvergenceAudioManager::ClearAllBeatSlots()
{
	if (ProceduralSynthComponent)
	{
		ProceduralSynthComponent->ClearAllRhythmicPulseSlots();
	}

	for (AHarmonicStationActor* Station : Stations)
	{
		if (Station)
		{
			Station->ResetStation();
		}
	}
}

void AHarmonicConvergenceAudioManager::UpdateAttractMode(float DeltaTime, int32 ActiveLoopers)
{
	if (!bEnableAttractMode)
	{
		return;
	}

	if (ActiveVoiceCount == 0 && ActiveLoopers == 0)
	{
		InactivityDurationSeconds += DeltaTime;
		if (InactivityDurationSeconds >= AttractModeTimeoutSeconds)
		{
			TimeSinceLastAttractChimeSeconds += DeltaTime;
			if (TimeSinceLastAttractChimeSeconds >= AttractChimeIntervalSeconds && !Stations.IsEmpty())
			{
				TimeSinceLastAttractChimeSeconds = 0.0f;
				const int32 RandomIdx = FMath::RandRange(0, Stations.Num() - 1);
				if (AHarmonicStationActor* Station = Stations[RandomIdx])
				{
					if (ProceduralSynthComponent)
					{
						ProceduralSynthComponent->TriggerAttractPing(Station->BaseFrequencyHz);
					}
					if (Station->CrystalActor)
					{
						Station->CrystalActor->TriggerBeatPulse(0.8f);
					}
					OnAttractPingTriggered.Broadcast(Station->ChannelName, Station->BaseFrequencyHz);
				}
			}
		}
	}
	else
	{
		InactivityDurationSeconds = 0.0f;
		TimeSinceLastAttractChimeSeconds = 0.0f;
	}
}
