#include "HarmonicConvergenceAudioManager.h"
#include "HarmonicConvergenceSynthComponent.h"
#include "InputManagerSubSystem.h"
#include "Kismet/GameplayStatics.h"

AHarmonicConvergenceAudioManager::AHarmonicConvergenceAudioManager()
{
	PrimaryActorTick.bCanEverTick = true;

	// Pure C++ Procedural Synth Component (Root)
	ProceduralSynthComponent = CreateDefaultSubobject<UHarmonicConvergenceSynthComponent>(TEXT("ProceduralConvergenceSynth"));
	SetRootComponent(ProceduralSynthComponent);

	// Optional MetaSound Audio Component (Attached)
	CentralConvergenceAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("CentralConvergenceAudio"));
	CentralConvergenceAudioComponent->SetupAttachment(RootComponent);
	CentralConvergenceAudioComponent->bAutoActivate = false;
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

	if (ProceduralSynthComponent && !ProceduralSynthComponent->IsPlaying())
	{
		ProceduralSynthComponent->Start();
	}

	if (CentralConvergenceAudioComponent && CentralConvergenceAudioComponent->GetSound())
	{
		CentralConvergenceAudioComponent->Play();
	}
}

void AHarmonicConvergenceAudioManager::InitializeDefaultStations()
{
	// 20-Station Tiered Sound Design for Enschede Lights Up
	struct FStationInitData
	{
		const TCHAR* Name;
		EVoiceTimbreProfile Timbre;
		FLinearColor Color;
	};

	const FStationInitData StationTable[20] = {
		{ TEXT("player1"),  EVoiceTimbreProfile::SubBassPad,        FLinearColor(0.0f, 0.9f, 1.0f) }, // Cyan (C2 Sub)
		{ TEXT("player2"),  EVoiceTimbreProfile::WarmPad,           FLinearColor(1.0f, 0.5f, 0.0f) }, // Amber (G3)
		{ TEXT("player3"),  EVoiceTimbreProfile::WarmPad,           FLinearColor(1.0f, 0.0f, 0.8f) }, // Magenta (D4)
		{ TEXT("player4"),  EVoiceTimbreProfile::CrystallineChime,  FLinearColor(0.1f, 1.0f, 0.4f) }, // Emerald (G5 Chime)
		{ TEXT("player5"),  EVoiceTimbreProfile::SubBassPad,        FLinearColor(0.0f, 0.6f, 1.0f) }, // Azure (C3)
		{ TEXT("player6"),  EVoiceTimbreProfile::WarmPad,           FLinearColor(1.0f, 0.8f, 0.0f) }, // Gold (E4)
		{ TEXT("player7"),  EVoiceTimbreProfile::CrystallineChime,  FLinearColor(0.7f, 0.2f, 1.0f) }, // Violet (B5 Chime)
		{ TEXT("player8"),  EVoiceTimbreProfile::WarmPad,           FLinearColor(0.2f, 0.9f, 0.8f) }, // Turquoise (G4)
		{ TEXT("player9"),  EVoiceTimbreProfile::CrystallineChime,  FLinearColor(1.0f, 0.4f, 0.6f) }, // Rose (D6 Chime)
		{ TEXT("player10"), EVoiceTimbreProfile::SubBassPad,        FLinearColor(0.3f, 0.0f, 0.9f) }, // Deep Indigo (G2 Sub)
		{ TEXT("player11"), EVoiceTimbreProfile::WarmPad,           FLinearColor(0.9f, 0.9f, 0.2f) }, // Sunbeam (A4)
		{ TEXT("player12"), EVoiceTimbreProfile::CrystallineChime,  FLinearColor(0.2f, 1.0f, 0.7f) }, // Mint (E6 Chime)
		{ TEXT("player13"), EVoiceTimbreProfile::WarmPad,           FLinearColor(1.0f, 0.2f, 0.2f) }, // Crimson (C5)
		{ TEXT("player14"), EVoiceTimbreProfile::WarmPad,           FLinearColor(1.0f, 0.6f, 0.3f) }, // Coral (D5)
		{ TEXT("player15"), EVoiceTimbreProfile::CrystallineChime,  FLinearColor(0.5f, 0.9f, 1.0f) }, // Ice Blue (G6 Chime)
		{ TEXT("player16"), EVoiceTimbreProfile::SubBassPad,        FLinearColor(0.8f, 0.5f, 0.0f) }, // Bronze (G3 Bass)
		{ TEXT("player17"), EVoiceTimbreProfile::WarmPad,           FLinearColor(0.9f, 0.1f, 0.6f) }, // Orchid (E5)
		{ TEXT("player18"), EVoiceTimbreProfile::CrystallineChime,  FLinearColor(1.0f, 1.0f, 1.0f) }, // Starlight White (C7 Chime)
		{ TEXT("player19"), EVoiceTimbreProfile::WarmPad,           FLinearColor(0.1f, 0.7f, 1.0f) }, // Sky Blue (C4)
		{ TEXT("player20"), EVoiceTimbreProfile::VortexSweep,       FLinearColor(0.8f, 0.0f, 1.0f) }  // Celestial Purple (Vortex)
	};

	StationConfigs.Empty();
	for (int32 i = 0; i < 20; ++i)
	{
		FStationVoiceConfig Config;
		Config.ChannelName = FName(StationTable[i].Name);
		Config.TimbreProfile = StationTable[i].Timbre;
		Config.StreamColor = StationTable[i].Color;
		
		// 360-degree perimeter circular panning around the pit railing
		const float Angle = (i / 20.0f) * 2.0f * PI;
		Config.PanPosition = FMath::Sin(Angle); // Smooth circular stereo pan

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
	// 20-Note Open Harmonic Voicings across 5 octaves (Zero Clashing / Pure Consonance)
	// Open Major Celestial (C2, G3, D4, G5, C3, E4, B5, G4, D6, G2, A4, E6, C5, D5, G6, G3, E5, C7, C4, G4)
	static const float OpenMajorFreqs[20] = {
		65.41f, 196.00f, 293.66f, 783.99f, 130.81f, 329.63f, 987.77f, 392.00f, 1174.66f, 97.99f,
		440.00f, 1318.51f, 523.25f, 587.33f, 1567.98f, 196.00f, 659.25f, 2093.00f, 261.63f, 392.00f
	};

	// Open Lydian Dream
	static const float OpenLydianFreqs[20] = {
		65.41f, 196.00f, 293.66f, 783.99f, 130.81f, 369.99f, 987.77f, 392.00f, 1174.66f, 97.99f,
		493.88f, 1318.51f, 587.33f, 739.99f, 1567.98f, 196.00f, 659.25f, 2093.00f, 261.63f, 369.99f
	};

	// Open Ambient Minor (Cm9 / Luminous Night)
	static const float OpenMinorFreqs[20] = {
		65.41f, 196.00f, 311.13f, 783.99f, 130.81f, 349.23f, 932.33f, 392.00f, 1174.66f, 97.99f,
		466.16f, 1244.51f, 523.25f, 587.33f, 1567.98f, 196.00f, 622.25f, 2093.00f, 261.63f, 392.00f
	};

	// Open Hirajoshi Luminous
	static const float OpenHirajoshiFreqs[20] = {
		65.41f, 196.00f, 277.18f, 783.99f, 130.81f, 349.23f, 830.61f, 392.00f, 1046.50f, 97.99f,
		415.30f, 1108.73f, 523.25f, 554.37f, 1567.98f, 196.00f, 698.46f, 2093.00f, 261.63f, 349.23f
	};

	// Open Dorian Horizon
	static const float OpenDorianFreqs[20] = {
		65.41f, 196.00f, 293.66f, 783.99f, 130.81f, 349.23f, 880.00f, 392.00f, 1174.66f, 97.99f,
		440.00f, 1318.51f, 523.25f, 587.33f, 1567.98f, 196.00f, 659.25f, 2093.00f, 261.63f, 440.00f
	};

	for (int32 i = 0; i < StationConfigs.Num(); ++i)
	{
		switch (ScaleMode)
		{
		case EHarmonicScaleMode::PentatonicMajor:
			StationConfigs[i].BaseFrequencyHz = OpenMajorFreqs[i % 20];
			break;
		case EHarmonicScaleMode::LydianCelestial:
			StationConfigs[i].BaseFrequencyHz = OpenLydianFreqs[i % 20];
			break;
		case EHarmonicScaleMode::PentatonicMinor:
			StationConfigs[i].BaseFrequencyHz = OpenMinorFreqs[i % 20];
			break;
		case EHarmonicScaleMode::HirajoshiLuminous:
			StationConfigs[i].BaseFrequencyHz = OpenHirajoshiFreqs[i % 20];
			break;
		case EHarmonicScaleMode::DorianAmbient:
			StationConfigs[i].BaseFrequencyHz = OpenDorianFreqs[i % 20];
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

	int32 VoiceIndex = INDEX_NONE;
	float VoicePan = 0.0f;
	EVoiceTimbreProfile Timbre = EVoiceTimbreProfile::WarmPad;

	for (int32 i = 0; i < StationConfigs.Num(); ++i)
	{
		if (StationConfigs[i].ChannelName == Channel)
		{
			VoiceIndex = i;
			VoicePan = StationConfigs[i].PanPosition;
			Timbre = StationConfigs[i].TimbreProfile;
			break;
		}
	}

	if (FVoiceRuntimeState* State = VoiceStates.Find(Channel))
	{
		State->bIsPressed = true;
		State->CurrentHoldDuration = 0.0f;
		State->CurrentPressure = 1.0f;

		// 1. Direct Pure C++ Procedural Synth Trigger with Timbre Profile
		if (ProceduralSynthComponent && VoiceIndex != INDEX_NONE)
		{
			ProceduralSynthComponent->NoteOn(VoiceIndex, State->CurrentFrequency, VoicePan, Timbre);
		}

		// 2. Optional MetaSound Trigger
		if (CentralConvergenceAudioComponent && CentralConvergenceAudioComponent->IsPlaying())
		{
			FString TriggerName = FString::Printf(TEXT("Trigger_%s_On"), *Channel.ToString());
			CentralConvergenceAudioComponent->SetTriggerParameter(FName(*TriggerName));
		}
	}
}

void AHarmonicConvergenceAudioManager::TriggerStationNoteOff(FName Channel)
{
	IdleTimer = 0.0f;

	int32 VoiceIndex = INDEX_NONE;
	for (int32 i = 0; i < StationConfigs.Num(); ++i)
	{
		if (StationConfigs[i].ChannelName == Channel)
		{
			VoiceIndex = i;
			break;
		}
	}

	if (FVoiceRuntimeState* State = VoiceStates.Find(Channel))
	{
		if (State->bIsPressed)
		{
			if (State->CurrentHoldDuration >= ShockwaveThresholdHoldTime)
			{
				OnShockwaveReleased.Broadcast(Channel, State->CurrentHoldDuration);
				
				if (ProceduralSynthComponent)
				{
					ProceduralSynthComponent->TriggerShockwave();
				}
				if (CentralConvergenceAudioComponent && CentralConvergenceAudioComponent->IsPlaying())
				{
					CentralConvergenceAudioComponent->SetTriggerParameter(TEXT("Trigger_Shockwave"));
				}
			}

			State->bIsPressed = false;
			State->ModulationIntensity = 0.0f;
			State->CurrentPressure = 0.0f;

			// 1. Direct Pure C++ Procedural Synth Trigger
			if (ProceduralSynthComponent && VoiceIndex != INDEX_NONE)
			{
				ProceduralSynthComponent->NoteOff(VoiceIndex);
			}

			// 2. Optional MetaSound Trigger
			if (CentralConvergenceAudioComponent && CentralConvergenceAudioComponent->IsPlaying())
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

	for (int32 i = 0; i < StationConfigs.Num(); ++i)
	{
		const FName Channel = StationConfigs[i].ChannelName;
		if (FVoiceRuntimeState* State = VoiceStates.Find(Channel))
		{
			if (State->bIsPressed)
			{
				ActiveVoiceCount++;
				State->CurrentHoldDuration += DeltaTime;

				// Dynamic modulation builds over 4 seconds, scaled by pressure
				const float BaseMod = FMath::Clamp(State->CurrentHoldDuration / 4.0f, 0.0f, 1.0f);
				State->ModulationIntensity = FMath::Max(BaseMod, State->CurrentPressure * 0.5f);
				TotalModulation += State->ModulationIntensity;

				if (ProceduralSynthComponent)
				{
					ProceduralSynthComponent->SetVoiceModulation(i, State->ModulationIntensity);
				}
			}
		}
	}

	const int32 TotalConfigured = FMath::Max(1, StationConfigs.Num());
	// Exponential perceived energy curve: 1 player gives gentle energy, 4 players give massive crescendo
	const float NormalizedActive = (ActiveVoiceCount > 0) ? (ActiveVoiceCount / (float)TotalConfigured) : 0.0f;
	const float ExponentialEnergy = FMath::Pow(NormalizedActive, 1.35f) * 2.0f;
	const float TargetEnergy = ExponentialEnergy * (1.0f + 0.35f * TotalModulation);

	// Responsive smooth interpolation
	ConvergenceEnergy = FMath::FInterpTo(ConvergenceEnergy, TargetEnergy, DeltaTime, 5.0f);

	if (ProceduralSynthComponent)
	{
		ProceduralSynthComponent->SetConvergenceEnergy(ConvergenceEnergy);
	}

	// Harmonic Crescendo detection (when all stations held > CrescendoRequiredHoldTime)
	if (ActiveVoiceCount == StationConfigs.Num() && StationConfigs.Num() > 1)
	{
		AllStationsActiveTimer += DeltaTime;
		if (AllStationsActiveTimer >= CrescendoRequiredHoldTime && !bCrescendoActive)
		{
			bCrescendoActive = true;
			OnHarmonicCrescendo.Broadcast(ConvergenceEnergy);
			if (CentralConvergenceAudioComponent && CentralConvergenceAudioComponent->IsPlaying())
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
				if (StationConfigs.Num() > 0)
				{
					const int32 RandomIndex = FMath::RandRange(0, StationConfigs.Num() - 1);
					const float ChimeFreq = StationConfigs[RandomIndex].BaseFrequencyHz;

					if (ProceduralSynthComponent)
					{
						ProceduralSynthComponent->TriggerAttractPing(ChimeFreq);
					}

					if (CentralConvergenceAudioComponent && CentralConvergenceAudioComponent->IsPlaying())
					{
						CentralConvergenceAudioComponent->SetFloatParameter(TEXT("AttractFrequency"), ChimeFreq);
						CentralConvergenceAudioComponent->SetTriggerParameter(TEXT("Trigger_AttractPing"));
					}
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
	if (!CentralConvergenceAudioComponent || !CentralConvergenceAudioComponent->IsPlaying())
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
