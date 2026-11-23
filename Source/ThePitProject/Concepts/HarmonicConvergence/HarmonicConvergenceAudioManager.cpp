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
			Station->VortexForce = DefaultVortexForce;
			Station->TrickleVortexForce = TrickleVortexForce;
			if (!Station->bUseCustomSpawnRate)
			{
				Station->SpawnRate = StationBeamSpawnRate;
				Station->TrickleSpawnMultiplier = StationTrickleMultiplier;
			}
			if (Station->BeamComponent)
			{
				Station->BeamComponent->SetVariableFloat(FName(TEXT("User.myVortexForce")), Station->VortexForce);
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
	LastSyncedHardwareBPM = MasterBPM;

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

	// Auto-Swap: If physical station unit exceeds MaxActiveCrystalsPerStation (or global active crystals exceed MaxActiveCrystals), deactivate the oldest active crystal
	const int32 UnitIdx = Station->StationIndex / 4;
	TArray<AHarmonicStationActor*> UnitActiveStations;
	TArray<AHarmonicStationActor*> AllOtherActiveStations;

	for (AHarmonicStationActor* OtherStation : Stations)
	{
		if (OtherStation && OtherStation != Station && OtherStation->IsCrystalActive())
		{
			AllOtherActiveStations.Add(OtherStation);
			if ((OtherStation->StationIndex / 4) == UnitIdx)
			{
				UnitActiveStations.Add(OtherStation);
			}
		}
	}

	auto DeactivateOldestIn = [](const TArray<AHarmonicStationActor*>& Candidates)
	{
		AHarmonicStationActor* OldestStation = nullptr;
		float LowestRemainingLifespan = MAX_flt;
		for (AHarmonicStationActor* S : Candidates)
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
	};

	if (UnitActiveStations.Num() >= MaxActiveCrystalsPerStation)
	{
		DeactivateOldestIn(UnitActiveStations);
	}
	else if (AllOtherActiveStations.Num() >= MaxActiveCrystals)
	{
		DeactivateOldestIn(AllOtherActiveStations);
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

	// Simultaneous Co-op Ignition Shockwave: if 2 distinct stations lock in within 0.6s of each other, pulse a light wave around the ring and chime an octave harmonic
	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (LastActivatedStationIndex != INDEX_NONE && LastActivatedStationIndex != Station->StationIndex && (CurrentTime - LastCrystalActivationTimestamp) <= 0.60f)
	{
		if (ProceduralSynthComponent)
		{
			const float ComboFreqHz = ProceduralSynthComponent->GetChordVoiceFrequencyHz(GetEffectiveVoiceRole(Station), Station->OctaveOffset + 1, Station->BaseFrequencyHz * 2.0f);
			ProceduralSynthComponent->TriggerEchoRipple(ComboFreqHz, Station->PanPosition, Station->StationIndex);
		}
		for (AHarmonicStationActor* RingStation : Stations)
		{
			if (RingStation && RingStation->CrystalActor)
			{
				RingStation->CrystalActor->TriggerBeatPulse(0.95f);
			}
		}
	}
	LastCrystalActivationTimestamp = CurrentTime;
	LastActivatedStationIndex = Station->StationIndex;
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
		ProceduralSynthComponent->TriggerEchoRipple(LiveFreqHz * 2.0f, Station->PanPosition, Station->StationIndex);
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
	int32 ActiveLoopingCrystalCount = 0;
	int32 CombinedActiveSourceCount = 0;
	float TotalModulationAccumulator = 0.0f;
	int32 TimbreCounts[4] = { 0, 0, 0, 0 };

	for (AHarmonicStationActor* Station : Stations)
	{
		if (!Station)
		{
			continue;
		}

		const bool bCrystalLooping = Station->IsCrystalActive();
		const int32 TimbreIdx = FMath::Clamp(static_cast<int32>(Station->TimbreProfile), 0, 3);

		if (bCrystalLooping)
		{
			ActiveLoopingCrystalCount++;
			CombinedActiveSourceCount++;
			TimbreCounts[TimbreIdx]++;

			if (ProceduralSynthComponent)
			{
				const float AudioFadeAlpha = FMath::Clamp(Station->CrystalActor->RemainingLifespan / 6.0f, 0.0f, 1.0f);
				ProceduralSynthComponent->SetRhythmicSlotGain(Station->StationIndex, AudioFadeAlpha);
			}
		}

		if (Station->bIsPressed)
		{
			ActiveVoiceCount++;
			const float ChargeProgress = Station->GetChargeProgress();
			TotalModulationAccumulator += ChargeProgress;
			if (!bCrystalLooping)
			{
				CombinedActiveSourceCount++;
				TimbreCounts[TimbreIdx]++;
			}
			if (ProceduralSynthComponent)
			{
				ProceduralSynthComponent->SetVoiceModulation(Station->StationIndex, ChargeProgress);
			}
		}

		Station->UpdateBeam();
	}

	UpdateModalHarmonyAndVisuals(DeltaTime, TimbreCounts, CombinedActiveSourceCount);

	float TargetConvergenceEnergy = 0.0f;
	if (CombinedActiveSourceCount > 0)
	{
		const float SteppedIntensity = BaseActiveIntensity + static_cast<float>(CombinedActiveSourceCount - 1) * IntensityStepPerCrystal;
		const float HoldBoost = (ActiveVoiceCount > 0) ? FMath::Min(0.25f, TotalModulationAccumulator * 0.1f) : 0.0f;
		TargetConvergenceEnergy = FMath::Clamp(SteppedIntensity + HoldBoost, 0.0f, 2.0f);
	}
	ConvergenceEnergy = FMath::FInterpTo(ConvergenceEnergy, TargetConvergenceEnergy, DeltaTime, 5.0f);

	if (ProceduralSynthComponent)
	{
		ProceduralSynthComponent->SetConvergenceEnergy(ConvergenceEnergy);
	}

	const bool bShouldOverdrive = (ActiveLoopingCrystalCount >= OverdriveThreshold);
	if (bShouldOverdrive != bIsHarmonicOverdrive)
	{
		bIsHarmonicOverdrive = bShouldOverdrive;
		if (bIsHarmonicOverdrive && ProceduralSynthComponent)
		{
			ProceduralSynthComponent->TriggerOverdriveDrop();
		}
	}

	const float TargetOverdrive = bIsHarmonicOverdrive ? FMath::Clamp((ActiveLoopingCrystalCount - OverdriveThreshold + 1) / 4.0f, 0.45f, 1.0f) : 0.0f;
	OverdriveIntensity = FMath::FInterpTo(OverdriveIntensity, TargetOverdrive, DeltaTime, 3.0f);

	if (ProceduralSynthComponent)
	{
		ProceduralSynthComponent->SetOverdriveIntensity(OverdriveIntensity);
	}

	UpdateAttractMode(DeltaTime, ActiveLoopingCrystalCount);
}

FLinearColor AHarmonicConvergenceAudioManager::GetModalAtmosphereColor(EHarmonicScaleMode Mode, int32 UnitIdx) const
{
	const bool bIsAlternateUnit = (UnitIdx % 2 == 1);
	switch (Mode)
	{
	case EHarmonicScaleMode::AeolianMinor:
		return bIsAlternateUnit ? FLinearColor(0.55f, 0.10f, 0.85f) : FLinearColor(0.15f, 0.20f, 0.95f);
	case EHarmonicScaleMode::DorianAmbient:
		return bIsAlternateUnit ? FLinearColor(0.95f, 0.15f, 0.65f) : FLinearColor(0.0f, 0.95f, 0.90f);
	case EHarmonicScaleMode::IonianMajor:
		return bIsAlternateUnit ? FLinearColor(1.0f, 0.55f, 0.15f) : FLinearColor(1.0f, 0.92f, 0.35f);
	case EHarmonicScaleMode::LydianCelestial:
	default:
		return bIsAlternateUnit ? FLinearColor(0.20f, 0.90f, 1.0f) : FLinearColor(0.15f, 1.0f, 0.60f);
	}
}

void AHarmonicConvergenceAudioManager::UpdateModalHarmonyAndVisuals(float DeltaTime, const int32 TimbreCounts[4], int32 CombinedActiveSources)
{
	const EHarmonicScaleMode PreviousScaleMode = ScaleMode;
	const int32 ActiveWarmPads = TimbreCounts[static_cast<int32>(EVoiceTimbreProfile::WarmPad)];
	const int32 ActiveSubBasses = TimbreCounts[static_cast<int32>(EVoiceTimbreProfile::SubBassPad)];
	const int32 ActiveChimes = TimbreCounts[static_cast<int32>(EVoiceTimbreProfile::CrystallineChime)];
	const int32 ActiveVortexSweeps = TimbreCounts[static_cast<int32>(EVoiceTimbreProfile::VortexSweep)];

	if (ProceduralSynthComponent)
	{
		const float LydianBias  = 1.0f + (ActiveChimes + ActiveVortexSweeps) * 1.35f;
		const float DorianBias  = 1.0f + ActiveSubBasses * 1.40f;
		const float AeolianBias = 1.0f + ActiveWarmPads * 1.35f;
		const bool bHasFullSectionCollaboration = (ActiveWarmPads > 0 && ActiveSubBasses > 0 && (ActiveChimes + ActiveVortexSweeps) > 0);
		const float IonianBias  = bHasFullSectionCollaboration
			? (2.0f + (ActiveWarmPads + ActiveSubBasses + ActiveChimes + ActiveVortexSweeps) * 0.6f)
			: 0.5f;
		ProceduralSynthComponent->SetScaleModeBiasWeights(LydianBias, DorianBias, IonianBias, AeolianBias);

		if (bAutoScaleMode && ScaleMode != EHarmonicScaleMode::CustomFrequencies)
		{
			ScaleMode = ProceduralSynthComponent->GetActiveScaleMode();
		}
	}

	if (bEnableDynamicBPM)
	{
		const float SafeMinBPM = FMath::Min(MinDynamicBPM, MaxDynamicBPM);
		const float SafeMaxBPM = FMath::Max(MinDynamicBPM, MaxDynamicBPM);
		const float SpanBPM = SafeMaxBPM - SafeMinBPM;

		const float ModalTempoFraction =
			(ScaleMode == EHarmonicScaleMode::AeolianMinor)    ? 0.10f :
			(ScaleMode == EHarmonicScaleMode::LydianCelestial) ? 0.38f :
			(ScaleMode == EHarmonicScaleMode::DorianAmbient)   ? 0.65f : 0.80f;

		const float IdleBaseBPM = SafeMinBPM + SpanBPM * 0.15f;
		const float ActiveModalBPM = SafeMinBPM + SpanBPM * ModalTempoFraction;
		const float TimbreTempoDelta = (ActiveSubBasses + ActiveChimes) * 2.2f + ActiveVortexSweeps * 1.0f - ActiveWarmPads * 1.0f;
		const float OverdriveRushBPM = bIsHarmonicOverdrive ? 6.0f : 0.0f;
		const float TargetBPM = (CombinedActiveSources > 0)
			? FMath::Clamp(ActiveModalBPM + TimbreTempoDelta + OverdriveRushBPM, SafeMinBPM, SafeMaxBPM)
			: IdleBaseBPM;

		MasterBPM = FMath::FInterpTo(MasterBPM, TargetBPM, DeltaTime, 0.85f);
		if (ProceduralSynthComponent)
		{
			ProceduralSynthComponent->SetMasterBPM(MasterBPM);
		}
		if (FMath::Abs(MasterBPM - LastSyncedHardwareBPM) >= 2.0f)
		{
			SyncHardwareConfig();
		}
	}

	const float TargetVortexForce =
		(ScaleMode == EHarmonicScaleMode::IonianMajor)     ? IonianVortexForce :
		(ScaleMode == EHarmonicScaleMode::LydianCelestial) ? LydianVortexForce :
		(ScaleMode == EHarmonicScaleMode::DorianAmbient)   ? DorianVortexForce : AeolianVortexForce;

	for (AHarmonicStationActor* Station : Stations)
	{
		if (!Station)
		{
			continue;
		}

		Station->VortexForce = FMath::FInterpTo(Station->VortexForce, TargetVortexForce, DeltaTime, 2.0f);

		const int32 UnitIdx = FMath::Clamp(Station->StationIndex / 4, 0, 5);
		const FLinearColor ThemeColor = GetModalAtmosphereColor(ScaleMode, UnitIdx);
		const FLinearColor TargetColor = FLinearColor::LerpUsingHSV(Station->StreamColor, ThemeColor, VisualThemeBlendStrength);

		FLinearColor CurrentColor = TargetColor;
		if (Station->CrystalActor)
		{
			Station->CrystalActor->CrystalColor = FMath::CInterpTo(Station->CrystalActor->CrystalColor, TargetColor, DeltaTime, 2.0f);
			CurrentColor = Station->CrystalActor->CrystalColor;
		}
		if (Station->BeamComponent)
		{
			const FVector DynamicColorVec = FVector(CurrentColor.R, CurrentColor.G, CurrentColor.B) * 30.0f;
			Station->BeamComponent->SetVariableVec3(FName(TEXT("User.BeamColor")), DynamicColorVec);
			Station->BeamComponent->SetVariableVec3(FName(TEXT("User.myColor")), DynamicColorVec);
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
