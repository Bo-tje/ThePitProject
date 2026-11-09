#include "HarmonicConvergenceAudioManager.h"
#include "HarmonicConvergenceSynthComponent.h"
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

int32 AHarmonicConvergenceAudioManager::GetCurrentChordProgressionIndex() const
{
	return ProceduralSynthComponent ? ProceduralSynthComponent->GetCurrentChordIndex() : 0;
}

AHarmonicStationActor* AHarmonicConvergenceAudioManager::FindStationByChannel(FName StationChannelName) const
{
	for (AHarmonicStationActor* Station : Stations)
	{
		if (Station && Station->ChannelName == StationChannelName)
		{
			return Station;
		}
	}
	return nullptr;
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
			Station->InitializeAudioStem(MasterQuartzClock);
			Station->OnStationPressedChanged.AddDynamic(this, &AHarmonicConvergenceAudioManager::HandleStationPressedChanged);
			if (Station->CrystalActor)
			{
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
		Stations.Sort([](const AHarmonicStationActor& A, const AHarmonicStationActor& B)
		{
			return A.StationIndex < B.StationIndex;
		});
	}
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

	static constexpr float ScaleFreqs[5][20] = {
		// PentatonicMajor
		{ 65.41f, 196.00f, 293.66f, 783.99f, 130.81f, 329.63f, 987.77f, 392.00f, 1174.66f, 98.00f,
		  440.00f, 1318.51f, 523.25f, 587.33f, 1567.98f, 196.00f, 659.25f, 2093.00f, 261.63f, 1046.50f },
		// PentatonicMinor
		{ 65.41f, 196.00f, 311.13f, 783.99f, 130.81f, 349.23f, 932.33f, 392.00f, 1244.51f, 98.00f,
		  466.16f, 1396.91f, 523.25f, 622.25f, 1567.98f, 196.00f, 698.46f, 2093.00f, 261.63f, 1046.50f },
		// LydianCelestial
		{ 65.41f, 196.00f, 293.66f, 739.99f, 130.81f, 329.63f, 987.77f, 392.00f, 1174.66f, 98.00f,
		  440.00f, 1479.98f, 523.25f, 587.33f, 1760.00f, 196.00f, 659.25f, 2093.00f, 261.63f, 1046.50f },
		// HirajoshiLuminous
		{ 65.41f, 196.00f, 277.18f, 783.99f, 130.81f, 349.23f, 880.00f, 392.00f, 1108.73f, 98.00f,
		  415.30f, 1396.91f, 523.25f, 554.37f, 1567.98f, 196.00f, 698.46f, 2093.00f, 261.63f, 1046.50f },
		// DorianAmbient
		{ 65.41f, 196.00f, 293.66f, 783.99f, 130.81f, 329.63f, 932.33f, 392.00f, 1174.66f, 98.00f,
		  440.00f, 1318.51f, 523.25f, 587.33f, 1567.98f, 196.00f, 659.25f, 2093.00f, 261.63f, 1046.50f }
	};

	const int32 ModeIdx = FMath::Clamp(static_cast<int32>(ScaleMode), 0, 4);
	for (AHarmonicStationActor* Station : Stations)
	{
		if (Station)
		{
			Station->BaseFrequencyHz = ScaleFreqs[ModeIdx][Station->StationIndex % 20];
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

	const bool bCanMoreActivate = (CurrentlyActiveLoopers < MaxActiveCrystals);

	for (AHarmonicStationActor* Station : Stations)
	{
		if (!Station)
		{
			continue;
		}

		if (Station->CrystalActor)
		{
			Station->CrystalActor->bCanActivate = bCanMoreActivate;
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

		if (Station->IsCrystalActive() && ProceduralSynthComponent && !Station->AudioStemAsset)
		{
			const float Remaining = Station->CrystalActor->RemainingLifespan;
			const float AudioFadeAlpha = (Remaining < 6.0f) ? FMath::Clamp(Remaining / 6.0f, 0.0f, 1.0f) : 1.0f;
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

	// Global Convergence Energy
	const int32 TotalStations = FMath::Max(1, Stations.Num());
	const float ActiveStationRatio = (ActiveVoiceCount > 0) ? (ActiveVoiceCount / static_cast<float>(TotalStations)) : 0.0f;
	const float TargetConvergenceEnergy = FMath::Pow(ActiveStationRatio, 1.35f) * 2.0f * (1.0f + 0.35f * TotalModulationAccumulator);
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
