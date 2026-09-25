#include "HarmonicConvergenceAudioManager.h"
#include "InputManagerSubSystem.h"
#include "Kismet/GameplayStatics.h"

AHarmonicConvergenceAudioManager::AHarmonicConvergenceAudioManager()
{
	PrimaryActorTick.bCanEverTick = true;

	CentralConvergenceAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("CentralConvergenceAudio"));
	SetRootComponent(CentralConvergenceAudioComponent);
}

void AHarmonicConvergenceAudioManager::BeginPlay()
{
	Super::BeginPlay();

	if (StationConfigs.Num() == 0)
	{
		InitializeDefaultStations();
	}
	ApplyHarmonicScaleFrequencies();

	VoiceStates.Empty();
	for (const FStationVoiceConfig& Config : StationConfigs)
	{
		FVoiceRuntimeState State;
		State.CurrentFrequency = Config.BaseFrequencyHz;
		VoiceStates.Add(Config.ChannelName, State);
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		InputSubsystem = GI->GetSubsystem<UInputManagerSubSystem>();
		if (InputSubsystem.IsValid())
		{
			InputSubsystem->OnButtonPressed.AddDynamic(this, &AHarmonicConvergenceAudioManager::HandleOSCButtonPressed);
			InputSubsystem->OnButtonReleased.AddDynamic(this, &AHarmonicConvergenceAudioManager::HandleOSCButtonReleased);
			InputSubsystem->OnInputChanged.AddDynamic(this, &AHarmonicConvergenceAudioManager::HandleOSCInputChanged);
		}
	}

	if (CentralConvergenceAudioComponent && !CentralConvergenceAudioComponent->IsPlaying())
	{
		CentralConvergenceAudioComponent->Play();
	}
}

void AHarmonicConvergenceAudioManager::InitializeDefaultStations()
{
	const FName Channels[] = { TEXT("player1"), TEXT("player2"), TEXT("player3"), TEXT("player4") };
	const FLinearColor Colors[] = {
		FLinearColor(0.f, 0.9f, 1.f),    // Cyan
		FLinearColor(1.f, 0.6f, 0.f),    // Amber
		FLinearColor(1.f, 0.f, 0.8f),    // Magenta
		FLinearColor(0.1f, 1.f, 0.4f)    // Emerald
	};
	const float Pans[] = { -0.75f, -0.25f, 0.25f, 0.75f };

	for (int32 i = 0; i < 4; ++i)
	{
		FStationVoiceConfig Config;
		Config.ChannelName = Channels[i];
		Config.StreamColor = Colors[i];
		Config.PanPosition = Pans[i];
		StationConfigs.Add(Config);
	}
}

void AHarmonicConvergenceAudioManager::SetHarmonicScale(EHarmonicScaleMode NewMode)
{
	ScaleMode = NewMode;
	ApplyHarmonicScaleFrequencies();

	for (const FStationVoiceConfig& Config : StationConfigs)
	{
		if (FVoiceRuntimeState* State = VoiceStates.Find(Config.ChannelName))
		{
			State->CurrentFrequency = Config.BaseFrequencyHz;
		}
	}
}

void AHarmonicConvergenceAudioManager::ApplyHarmonicScaleFrequencies()
{
	// Pentatonic Major: C4, D4, E4, G4, A4, C5
	static const float PentatonicMajorFreqs[] = { 261.63f, 293.66f, 329.63f, 392.00f, 440.00f, 523.25f };
	// Pentatonic Minor: C4, Eb4, F4, G4, Bb4, C5
	static const float PentatonicMinorFreqs[] = { 261.63f, 311.13f, 349.23f, 392.00f, 466.16f, 523.25f };
	// Lydian Celestial: C4, E4, F#4, G4, B4, D5
	static const float LydianCelestialFreqs[] = { 261.63f, 329.63f, 369.99f, 392.00f, 493.88f, 587.33f };
	// Hirajoshi Luminous: C4, Db4, F4, G4, Ab4, C5
	static const float HirajoshiFreqs[] = { 261.63f, 277.18f, 349.23f, 392.00f, 415.30f, 523.25f };
	// Dorian Ambient: C4, D4, Eb4, F4, G4, A4, Bb4
	static const float DorianFreqs[] = { 261.63f, 293.66f, 311.13f, 349.23f, 392.00f, 440.00f, 466.16f };

	for (int32 i = 0; i < StationConfigs.Num(); ++i)
	{
		switch (ScaleMode)
		{
		case EHarmonicScaleMode::PentatonicMajor:
			StationConfigs[i].BaseFrequencyHz = PentatonicMajorFreqs[i % UE_ARRAY_COUNT(PentatonicMajorFreqs)];
			break;
		case EHarmonicScaleMode::PentatonicMinor:
			StationConfigs[i].BaseFrequencyHz = PentatonicMinorFreqs[i % UE_ARRAY_COUNT(PentatonicMinorFreqs)];
			break;
		case EHarmonicScaleMode::LydianCelestial:
			StationConfigs[i].BaseFrequencyHz = LydianCelestialFreqs[i % UE_ARRAY_COUNT(LydianCelestialFreqs)];
			break;
		case EHarmonicScaleMode::HirajoshiLuminous:
			StationConfigs[i].BaseFrequencyHz = HirajoshiFreqs[i % UE_ARRAY_COUNT(HirajoshiFreqs)];
			break;
		case EHarmonicScaleMode::DorianAmbient:
			StationConfigs[i].BaseFrequencyHz = DorianFreqs[i % UE_ARRAY_COUNT(DorianFreqs)];
			break;
		case EHarmonicScaleMode::CustomFrequencies:
		default:
			break;
		}
	}
}

void AHarmonicConvergenceAudioManager::HandleOSCButtonPressed(FName Channel)
{
	TriggerStationNoteOn(Channel);
}

void AHarmonicConvergenceAudioManager::HandleOSCButtonReleased(FName Channel)
{
	TriggerStationNoteOff(Channel);
}

void AHarmonicConvergenceAudioManager::HandleOSCInputChanged(FName Channel, float Value, float Delta)
{
	SetStationPressure(Channel, Value);
}

void AHarmonicConvergenceAudioManager::SetStationPressure(FName Channel, float Pressure)
{
	if (FVoiceRuntimeState* State = VoiceStates.Find(Channel))
	{
		State->CurrentPressure = FMath::Clamp(Pressure, 0.0f, 1.0f);
	}
}

void AHarmonicConvergenceAudioManager::TriggerStationNoteOn(FName Channel)
{
	IdleTimer = 0.0f; // Reset idle attract timer

	if (FVoiceRuntimeState* State = VoiceStates.Find(Channel))
	{
		State->bIsPressed = true;
		State->CurrentHoldDuration = 0.0f;
		State->CurrentPressure = 1.0f;

		// Trigger attack parameter on the central MetaSound
		if (CentralConvergenceAudioComponent)
		{
			FString TriggerName = FString::Printf(TEXT("Trigger_%s_On"), *Channel.ToString());
			CentralConvergenceAudioComponent->SetTriggerParameter(FName(*TriggerName));
		}
	}
}

void AHarmonicConvergenceAudioManager::TriggerStationNoteOff(FName Channel)
{
	IdleTimer = 0.0f;

	if (FVoiceRuntimeState* State = VoiceStates.Find(Channel))
	{
		if (State->bIsPressed)
		{
			if (State->CurrentHoldDuration >= ShockwaveThresholdHoldTime)
			{
				OnShockwaveReleased.Broadcast(Channel, State->CurrentHoldDuration);
				if (CentralConvergenceAudioComponent)
				{
					CentralConvergenceAudioComponent->SetTriggerParameter(TEXT("Trigger_Shockwave"));
				}
			}

			State->bIsPressed = false;
			State->ModulationIntensity = 0.0f;
			State->CurrentPressure = 0.0f;

			if (CentralConvergenceAudioComponent)
			{
				FString TriggerName = FString::Printf(TEXT("Trigger_%s_Off"), *Channel.ToString());
				CentralConvergenceAudioComponent->SetTriggerParameter(FName(*TriggerName));
			}
		}
	}
}

bool AHarmonicConvergenceAudioManager::GetVoiceState(FName Channel, FVoiceRuntimeState& OutState) const
{
	if (const FVoiceRuntimeState* State = VoiceStates.Find(Channel))
	{
		OutState = *State;
		return true;
	}
	return false;
}

void AHarmonicConvergenceAudioManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateConvergenceParameters(DeltaTime);
	UpdateAttractMode(DeltaTime);
	PushMetaSoundParameters();
}

void AHarmonicConvergenceAudioManager::UpdateConvergenceParameters(float DeltaTime)
{
	ActiveVoiceCount = 0;
	float TotalModulation = 0.0f;

	for (auto& Pair : VoiceStates)
	{
		FVoiceRuntimeState& State = Pair.Value;
		if (State.bIsPressed)
		{
			ActiveVoiceCount++;
			State.CurrentHoldDuration += DeltaTime;
			
			// Dynamic modulation builds over 4 seconds, scaled by pressure
			const float BaseMod = FMath::Clamp(State.CurrentHoldDuration / 4.0f, 0.0f, 1.0f);
			State.ModulationIntensity = FMath::Max(BaseMod, State.CurrentPressure * 0.5f);
			TotalModulation += State.ModulationIntensity;
		}
	}

	const int32 TotalConfigured = FMath::Max(1, StationConfigs.Num());
	// Exponential perceived energy curve: 1 player gives gentle energy, 4 players give massive crescendo
	const float NormalizedActive = (ActiveVoiceCount > 0) ? (ActiveVoiceCount / (float)TotalConfigured) : 0.0f;
	const float ExponentialEnergy = FMath::Pow(NormalizedActive, 1.35f) * 2.0f;
	const float TargetEnergy = ExponentialEnergy * (1.0f + 0.35f * TotalModulation);

	// Responsive smooth interpolation
	ConvergenceEnergy = FMath::FInterpTo(ConvergenceEnergy, TargetEnergy, DeltaTime, 5.0f);

	// Harmonic Crescendo detection (when all stations held > CrescendoRequiredHoldTime)
	if (ActiveVoiceCount == StationConfigs.Num() && StationConfigs.Num() > 1)
	{
		AllStationsActiveTimer += DeltaTime;
		if (AllStationsActiveTimer >= CrescendoRequiredHoldTime && !bCrescendoActive)
		{
			bCrescendoActive = true;
			OnHarmonicCrescendo.Broadcast(ConvergenceEnergy);
			if (CentralConvergenceAudioComponent)
			{
				CentralConvergenceAudioComponent->SetTriggerParameter(TEXT("Trigger_Crescendo"));
			}
		}
	}
	else
	{
		AllStationsActiveTimer = 0.0f;
		bCrescendoActive = false;
	}
}

void AHarmonicConvergenceAudioManager::UpdateAttractMode(float DeltaTime)
{
	if (!bEnableAttractMode) return;

	if (ActiveVoiceCount == 0)
	{
		IdleTimer += DeltaTime;
		if (IdleTimer >= AttractIdleThreshold)
		{
			ChimeTimer += DeltaTime;
			if (ChimeTimer >= AttractChimeInterval)
			{
				ChimeTimer = 0.0f;
				if (CentralConvergenceAudioComponent && StationConfigs.Num() > 0)
				{
					// Pick random note from scale for ambient invitation ping
					const int32 RandomIndex = FMath::RandRange(0, StationConfigs.Num() - 1);
					CentralConvergenceAudioComponent->SetFloatParameter(TEXT("AttractFrequency"), StationConfigs[RandomIndex].BaseFrequencyHz);
					CentralConvergenceAudioComponent->SetTriggerParameter(TEXT("Trigger_AttractPing"));
				}
			}
		}
	}
	else
	{
		IdleTimer = 0.0f;
		ChimeTimer = 0.0f;
	}
}

void AHarmonicConvergenceAudioManager::PushMetaSoundParameters()
{
	if (!CentralConvergenceAudioComponent)
	{
		return;
	}

	// Central aggregate parameters
	CentralConvergenceAudioComponent->SetIntParameter(TEXT("ActiveVoiceCount"), ActiveVoiceCount);
	CentralConvergenceAudioComponent->SetFloatParameter(TEXT("ActiveVoiceCount"), static_cast<float>(ActiveVoiceCount));
	CentralConvergenceAudioComponent->SetFloatParameter(TEXT("ConvergenceEnergy"), ConvergenceEnergy);

	// Per-voice frequency, modulation, and stereo pan parameters
	for (int32 i = 0; i < StationConfigs.Num(); ++i)
	{
		const FName Channel = StationConfigs[i].ChannelName;
		if (const FVoiceRuntimeState* State = VoiceStates.Find(Channel))
		{
			const FString FreqParam = FString::Printf(TEXT("%s_Frequency"), *Channel.ToString());
			const FString ModParam = FString::Printf(TEXT("%s_Modulation"), *Channel.ToString());
			const FString ActiveParam = FString::Printf(TEXT("%s_IsActive"), *Channel.ToString());
			const FString PanParam = FString::Printf(TEXT("%s_Pan"), *Channel.ToString());

			CentralConvergenceAudioComponent->SetFloatParameter(FName(*FreqParam), State->CurrentFrequency);
			CentralConvergenceAudioComponent->SetFloatParameter(FName(*ModParam), State->ModulationIntensity);
			CentralConvergenceAudioComponent->SetBoolParameter(FName(*ActiveParam), State->bIsPressed);
			CentralConvergenceAudioComponent->SetFloatParameter(FName(*PanParam), StationConfigs[i].PanPosition);
		}
	}
}
