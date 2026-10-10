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

void AHarmonicConvergenceAudioManager::SetHarmonicScale(EHarmonicScaleMode NewScaleMode)
{
	ScaleMode = NewScaleMode;
	ApplyHarmonicScaleFrequencies();

	for (const FStationVoiceConfig& StationConfig : StationConfigs)
	{
		if (FVoiceRuntimeState* VoiceState = VoiceStates.Find(StationConfig.ChannelName))
		{
			VoiceState->CurrentFrequency = StationConfig.BaseFrequencyHz;
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

	for (int32 StationIndex = 0; StationIndex < StationConfigs.Num(); ++StationIndex)
	{
		switch (ScaleMode)
		{
		case EHarmonicScaleMode::PentatonicMajor:
			StationConfigs[StationIndex].BaseFrequencyHz = OpenMajorFreqs[StationIndex % 20];
			break;
		case EHarmonicScaleMode::LydianCelestial:
			StationConfigs[StationIndex].BaseFrequencyHz = OpenLydianFreqs[StationIndex % 20];
			break;
		case EHarmonicScaleMode::PentatonicMinor:
			StationConfigs[StationIndex].BaseFrequencyHz = OpenMinorFreqs[StationIndex % 20];
			break;
		case EHarmonicScaleMode::HirajoshiLuminous:
			StationConfigs[StationIndex].BaseFrequencyHz = OpenHirajoshiFreqs[StationIndex % 20];
			break;
		case EHarmonicScaleMode::DorianAmbient:
			StationConfigs[StationIndex].BaseFrequencyHz = OpenDorianFreqs[StationIndex % 20];
			break;
		case EHarmonicScaleMode::CustomFrequencies:
		default:
			break;
		}
	}
}

void AHarmonicConvergenceAudioManager::HandleOSCButtonPressed(FName StationChannelName)
{
	TriggerStationNoteOn(StationChannelName);
}

void AHarmonicConvergenceAudioManager::HandleOSCButtonReleased(FName StationChannelName)
{
	TriggerStationNoteOff(StationChannelName);
}

void AHarmonicConvergenceAudioManager::HandleOSCInputChanged(FName StationChannelName, float InputValue, float ValueDelta)
{
	SetStationPressure(StationChannelName, InputValue);
}

void AHarmonicConvergenceAudioManager::SetStationPressure(FName StationChannelName, float NormalizedPressure)
{
	if (FVoiceRuntimeState* VoiceState = VoiceStates.Find(StationChannelName))
	{
		VoiceState->CurrentPressure = FMath::Clamp(NormalizedPressure, 0.0f, 1.0f);
	}
}

void AHarmonicConvergenceAudioManager::TriggerStationNoteOn(FName StationChannelName)
{
	InactivityDurationSeconds = 0.0f; // Reset idle inactivity timer

	int32 FoundVoiceIndex = INDEX_NONE;
	float StationVoicePan = 0.0f;
	EVoiceTimbreProfile StationTimbre = EVoiceTimbreProfile::WarmPad;

	for (int32 StationIndex = 0; StationIndex < StationConfigs.Num(); ++StationIndex)
	{
		if (StationConfigs[StationIndex].ChannelName == StationChannelName)
		{
			FoundVoiceIndex = StationIndex;
			StationVoicePan = StationConfigs[StationIndex].PanPosition;
			StationTimbre = StationConfigs[StationIndex].TimbreProfile;
			break;
		}
	}

	if (FVoiceRuntimeState* VoiceState = VoiceStates.Find(StationChannelName))
	{
		VoiceState->bIsPressed = true;
		VoiceState->CurrentHoldDuration = 0.0f;
		VoiceState->CurrentPressure = 1.0f;

		// 1. Direct Pure C++ Procedural Synth Trigger with Timbre Profile
		if (ProceduralSynthComponent && FoundVoiceIndex != INDEX_NONE)
		{
			ProceduralSynthComponent->NoteOn(FoundVoiceIndex, VoiceState->CurrentFrequency, StationVoicePan, StationTimbre);
		}

		// 2. Broadcast high-level station event for Niagara / Blueprints
		OnStationVoiceStarted.Broadcast(StationChannelName, VoiceState->CurrentFrequency);

		// 3. Optional MetaSound Trigger
		if (CentralConvergenceAudioComponent && CentralConvergenceAudioComponent->IsPlaying())
		{
			const FString TriggerName = FString::Printf(TEXT("Trigger_%s_On"), *StationChannelName.ToString());
			CentralConvergenceAudioComponent->SetTriggerParameter(FName(*TriggerName));
		}
	}
}

void AHarmonicConvergenceAudioManager::TriggerStationNoteOff(FName StationChannelName)
{
	InactivityDurationSeconds = 0.0f;

	int32 FoundVoiceIndex = INDEX_NONE;
	for (int32 StationIndex = 0; StationIndex < StationConfigs.Num(); ++StationIndex)
	{
		if (StationConfigs[StationIndex].ChannelName == StationChannelName)
		{
			FoundVoiceIndex = StationIndex;
			break;
		}
	}

	if (FVoiceRuntimeState* VoiceState = VoiceStates.Find(StationChannelName))
	{
		if (VoiceState->bIsPressed)
		{
			VoiceState->bIsPressed = false;
			VoiceState->ModulationIntensity = 0.0f;
			VoiceState->CurrentPressure = 0.0f;

			// 1. Direct Pure C++ Procedural Synth Trigger
			if (ProceduralSynthComponent && FoundVoiceIndex != INDEX_NONE)
			{
				ProceduralSynthComponent->NoteOff(FoundVoiceIndex);
			}

			// 2. Broadcast high-level station event for Niagara / Blueprints
			OnStationVoiceStopped.Broadcast(StationChannelName);

			// 3. Optional MetaSound Trigger
			if (CentralConvergenceAudioComponent && CentralConvergenceAudioComponent->IsPlaying())
			{
				const FString TriggerName = FString::Printf(TEXT("Trigger_%s_Off"), *StationChannelName.ToString());
				CentralConvergenceAudioComponent->SetTriggerParameter(FName(*TriggerName));
			}
		}
	}
}

bool AHarmonicConvergenceAudioManager::GetVoiceState(FName StationChannelName, FVoiceRuntimeState& OutState) const
{
	if (const FVoiceRuntimeState* VoiceState = VoiceStates.Find(StationChannelName))
	{
		OutState = *VoiceState;
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
	float TotalModulationIntensityAccumulator = 0.0f;

	for (int32 StationIndex = 0; StationIndex < StationConfigs.Num(); ++StationIndex)
	{
		const FName StationChannelName = StationConfigs[StationIndex].ChannelName;
		if (FVoiceRuntimeState* VoiceState = VoiceStates.Find(StationChannelName))
		{
			if (VoiceState->bIsPressed)
			{
				ActiveVoiceCount++;
				VoiceState->CurrentHoldDuration += DeltaTime;

				// Dynamic timbre modulation builds over 4 seconds of holding, scaled by analog pressure
				const float HoldProgressRatio = FMath::Clamp(VoiceState->CurrentHoldDuration / 4.0f, 0.0f, 1.0f);
				VoiceState->ModulationIntensity = FMath::Max(HoldProgressRatio, VoiceState->CurrentPressure * 0.5f);
				TotalModulationIntensityAccumulator += VoiceState->ModulationIntensity;

				if (ProceduralSynthComponent)
				{
					ProceduralSynthComponent->SetVoiceModulation(StationIndex, VoiceState->ModulationIntensity);
				}
			}
		}
	}

	const int32 TotalConfiguredStations = FMath::Max(1, StationConfigs.Num());
	// Exponential perceived energy curve: 1 player gives gentle energy, 4+ players give massive crescendo
	const float ActiveStationRatio = (ActiveVoiceCount > 0) ? (ActiveVoiceCount / static_cast<float>(TotalConfiguredStations)) : 0.0f;
	const float BaseConvergenceCurve = FMath::Pow(ActiveStationRatio, 1.35f) * 2.0f;
	const float TargetConvergenceEnergy = BaseConvergenceCurve * (1.0f + 0.35f * TotalModulationIntensityAccumulator);

	// Responsive smooth interpolation toward target energy
	ConvergenceEnergy = FMath::FInterpTo(ConvergenceEnergy, TargetConvergenceEnergy, DeltaTime, 5.0f);

	if (ProceduralSynthComponent)
	{
		ProceduralSynthComponent->SetConvergenceEnergy(ConvergenceEnergy);
	}
}

void AHarmonicConvergenceAudioManager::UpdateAttractMode(float DeltaTime)
{
	if (!bEnableAttractMode)
	{
		return;
	}

	if (ActiveVoiceCount == 0)
	{
		InactivityDurationSeconds += DeltaTime;
		if (InactivityDurationSeconds >= AttractIdleThreshold)
		{
			TimeSinceLastAttractChimeSeconds += DeltaTime;
			if (TimeSinceLastAttractChimeSeconds >= AttractChimeInterval)
			{
				TimeSinceLastAttractChimeSeconds = 0.0f;
				if (StationConfigs.Num() > 0)
				{
					const int32 RandomStationIndex = FMath::RandRange(0, StationConfigs.Num() - 1);
					const FName StationChannelName = StationConfigs[RandomStationIndex].ChannelName;
					const float ChimeFrequencyHz = StationConfigs[RandomStationIndex].BaseFrequencyHz;

					if (ProceduralSynthComponent)
					{
						ProceduralSynthComponent->TriggerAttractPing(ChimeFrequencyHz);
					}

					OnAttractPingTriggered.Broadcast(StationChannelName, ChimeFrequencyHz);

					if (CentralConvergenceAudioComponent && CentralConvergenceAudioComponent->IsPlaying())
					{
						CentralConvergenceAudioComponent->SetFloatParameter(TEXT("AttractFrequency"), ChimeFrequencyHz);
						CentralConvergenceAudioComponent->SetTriggerParameter(TEXT("Trigger_AttractPing"));
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
	for (int32 StationIndex = 0; StationIndex < StationConfigs.Num(); ++StationIndex)
	{
		const FName StationChannelName = StationConfigs[StationIndex].ChannelName;
		if (const FVoiceRuntimeState* VoiceState = VoiceStates.Find(StationChannelName))
		{
			const FString FreqParam = FString::Printf(TEXT("%s_Frequency"), *StationChannelName.ToString());
			const FString ModParam = FString::Printf(TEXT("%s_Modulation"), *StationChannelName.ToString());
			const FString ActiveParam = FString::Printf(TEXT("%s_IsActive"), *StationChannelName.ToString());
			const FString PanParam = FString::Printf(TEXT("%s_Pan"), *StationChannelName.ToString());

			CentralConvergenceAudioComponent->SetFloatParameter(FName(*FreqParam), VoiceState->CurrentFrequency);
			CentralConvergenceAudioComponent->SetFloatParameter(FName(*ModParam), VoiceState->ModulationIntensity);
			CentralConvergenceAudioComponent->SetBoolParameter(FName(*ActiveParam), VoiceState->bIsPressed);
			CentralConvergenceAudioComponent->SetFloatParameter(FName(*PanParam), StationConfigs[StationIndex].PanPosition);
		}
	}
}
