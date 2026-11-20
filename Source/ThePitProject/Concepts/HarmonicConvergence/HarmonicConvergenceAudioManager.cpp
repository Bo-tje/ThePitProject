#include "HarmonicConvergenceAudioManager.h"
#include "HarmonicConvergenceSynthComponent.h"
#include "InputManagerSubSystem.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"

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
	DiscoverStations();
	ApplyHarmonicScaleFrequencies();

	for (AHarmonicStationActor* Station : Stations)
	{
		if (Station)
		{
			Station->TapCutMaxDurationSeconds = TapCutMaxDurationSeconds;
			if (!Station->bUseCustomSpawnRate)
			{
				Station->SpawnRate = StationBeamSpawnRate;
				Station->TrickleSpawnMultiplier = StationTrickleMultiplier;
				if (Station->BeamComponent)
				{
					Station->BeamComponent->SetVariableFloat(FName(TEXT("User.myVortexForce")), Station->SpawnRate * 1.5f);
				}
			}
			Station->OnStationPressedChanged.AddDynamic(this, &AHarmonicConvergenceAudioManager::HandleStationPressedChanged);
			if (Station->CrystalActor)
			{
				Station->CrystalActor->ChargeThresholdSeconds = CrystalChargeTimeSeconds;
				Station->CrystalActor->ActiveLifespanSeconds = CrystalActiveLifespanSeconds;
				Station->CrystalActor->RemainingLifespan = CrystalActiveLifespanSeconds;
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
	if (ProceduralSynthComponent)
	{
		ProceduralSynthComponent->SetRootFrequencyHz(RootNoteFrequencyHz);
		ProceduralSynthComponent->SetScaleMode(ScaleMode);
	}
}

EChordVoiceRole AHarmonicConvergenceAudioManager::GetEffectiveVoiceRole(const AHarmonicStationActor* Station) const
{
	if (!Station || ScaleMode == EHarmonicScaleMode::CustomFrequencies)
	{
		return EChordVoiceRole::FixedPitch;
	}
	return Station->ChordVoiceRole;
}

void AHarmonicConvergenceAudioManager::HandleStationPressedChanged(AHarmonicStationActor* Station, bool bPressed)
{
	if (!Station)
	{
		return;
	}

	InactivityDurationSeconds = 0.0f;

	if (!ProceduralSynthComponent)
	{
		return;
	}

	if (bPressed)
	{
		ProceduralSynthComponent->NoteOn(Station->StationIndex, Station->BaseFrequencyHz, Station->PanPosition, GetEffectiveVoiceRole(Station), Station->OctaveOffset);
	}
	else
	{
		ProceduralSynthComponent->NoteOff(Station->StationIndex);
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
		if (OtherStation && OtherStation != Station && (OtherStation->StationIndex / 4) == UnitIdx && OtherStation->IsCrystalActive())
		{
			UnitActiveStations.Add(OtherStation);
		}
	}

	if (UnitActiveStations.Num() >= MaxActiveCrystalsPerStation)
	{
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
		ProceduralSynthComponent->SetRhythmicPulseSlot(
			Station->StationIndex,
			true,
			Station->RhythmicPatternMask,
			Station->BaseFrequencyHz,
			Station->PanPosition,
			Station->TimbreProfile,
			1.0f,
			GetEffectiveVoiceRole(Station),
			Station->OctaveOffset);
		ProceduralSynthComponent->TriggerVoiceIgnition(Station->StationIndex);
	}
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
		const float LiveFreqHz = ProceduralSynthComponent->GetChordVoiceFrequencyHz(GetEffectiveVoiceRole(Station), Station->OctaveOffset, Station->BaseFrequencyHz);
		ProceduralSynthComponent->TriggerEchoRipple(LiveFreqHz * 2.0f, Station->PanPosition);
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

		if (Station->IsCrystalActive() && ProceduralSynthComponent && Station->CrystalActor->RemainingLifespan < 6.0f)
		{
			const float AudioFadeAlpha = FMath::Clamp(Station->CrystalActor->RemainingLifespan / 6.0f, 0.0f, 1.0f);
			ProceduralSynthComponent->SetRhythmicSlotGain(Station->StationIndex, AudioFadeAlpha);
		}
	}

	const int32 CombinedActiveSources = FMath::Max(ActiveVoiceCount, CurrentlyActiveLoopers);
	float TargetConvergenceEnergy = 0.0f;
	if (CombinedActiveSources > 0)
	{
		const float SteppedIntensity = BaseActiveIntensity + static_cast<float>(CombinedActiveSources - 1) * IntensityStepPerCrystal;
		const float HoldBoost = (ActiveVoiceCount > 0) ? FMath::Min(0.25f, TotalModulationAccumulator * 0.1f) : 0.0f;
		TargetConvergenceEnergy = FMath::Clamp(SteppedIntensity + HoldBoost, 0.0f, 2.0f);
	}
	ConvergenceEnergy = FMath::FInterpTo(ConvergenceEnergy, TargetConvergenceEnergy, DeltaTime, 5.0f);

	for (AHarmonicStationActor* Station : Stations)
	{
		if (Station)
		{
			Station->UpdateBeam();
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
	}

	const float TargetOverdrive = bIsHarmonicOverdrive ? FMath::Clamp((CurrentlyActiveLoopers - OverdriveThreshold + 1) / 4.0f, 0.45f, 1.0f) : 0.0f;
	OverdriveIntensity = FMath::FInterpTo(OverdriveIntensity, TargetOverdrive, DeltaTime, 3.0f);

	if (ProceduralSynthComponent)
	{
		ProceduralSynthComponent->SetOverdriveIntensity(OverdriveIntensity);
	}

	UpdateAttractMode(DeltaTime, CurrentlyActiveLoopers);
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
						const float LiveFreqHz = ProceduralSynthComponent->GetChordVoiceFrequencyHz(GetEffectiveVoiceRole(Station), Station->OctaveOffset, Station->BaseFrequencyHz);
						ProceduralSynthComponent->TriggerAttractPing(LiveFreqHz);
					}
					if (Station->CrystalActor)
					{
						Station->CrystalActor->TriggerBeatPulse(0.8f);
					}
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
