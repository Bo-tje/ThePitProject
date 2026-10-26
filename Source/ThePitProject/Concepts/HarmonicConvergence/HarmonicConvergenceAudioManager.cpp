#include "HarmonicConvergenceAudioManager.h"
#include "HarmonicConvergenceSynthComponent.h"
#include "InputManagerSubSystem.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

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

	static ConstructorHelpers::FObjectFinderOptional<UNiagaraSystem> DefaultBeamSystem(TEXT("/Game/HarmonicConvergence/NS_HarmonicConvergence.NS_HarmonicConvergence"));
	if (DefaultBeamSystem.Succeeded())
	{
		StationBeamSystem = DefaultBeamSystem.Get();
	}

	static ConstructorHelpers::FObjectFinderOptional<UNiagaraSystem> DefaultCoreSystem(TEXT("/Game/HarmonicConvergence/NS_ConvergenceCore.NS_ConvergenceCore"));
	if (DefaultCoreSystem.Succeeded())
	{
		CenterVortexSystem = DefaultCoreSystem.Get();
	}
}

void AHarmonicConvergenceAudioManager::BeginPlay()
{
	if (StationConfigs.Num() == 0)
	{
		InitializeDefaultStations();
	}
	ApplyHarmonicScaleFrequencies();

	VoiceStates.Empty();
	StationBeamComponents.Empty();
	CenterVortexComponent = nullptr;

	// Initialize the 4 persistent rhythmic pulse slots with interlocking 4/4 step assignment: {0, 2, 1, 3}
	PersistentBeatSlots.SetNum(4);
	const int32 BeatStepOrder[4] = { 0, 2, 1, 3 };
	for (int32 SlotIdx = 0; SlotIdx < 4; ++SlotIdx)
	{
		PersistentBeatSlots[SlotIdx].SlotIndex = SlotIdx;
		PersistentBeatSlots[SlotIdx].AssignedBeatStep = BeatStepOrder[SlotIdx];
		PersistentBeatSlots[SlotIdx].bIsActive = false;
		PersistentBeatSlots[SlotIdx].RemainingLifespan = BeatLifespanSeconds;
	}

	// Ensure any legacy attached Niagara components on child Blueprints are deactivated before spawning per-station components
	TInlineComponentArray<UNiagaraComponent*> AttachedNiagaraComps(this);
	for (UNiagaraComponent* Comp : AttachedNiagaraComps)
	{
		if (Comp)
		{
			Comp->SetAutoActivate(false);
			Comp->DeactivateImmediate();
		}
	}

	const FVector CenterLoc = GetActorLocation();
	if (CenterVortexSystem)
	{
		CenterVortexComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), CenterVortexSystem, CenterLoc, FRotator::ZeroRotator, FVector(1.0f), false, true);
	}
	for (const FStationVoiceConfig& Config : StationConfigs)
	{
		FVoiceRuntimeState State;
		State.CurrentFrequency = Config.BaseFrequencyHz;
		VoiceStates.Add(Config.ChannelName, State);

		if (StationBeamSystem)
		{
			if (UNiagaraComponent* BeamComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				GetWorld(), StationBeamSystem, CenterLoc, FRotator::ZeroRotator, FVector(1.0f), false, false))
			{
				BeamComp->SetVariableVec3(FName(TEXT("User.CenterLocation")), CenterLoc);
				BeamComp->SetVariableVec3(FName(TEXT("User.BeamStartLocation")), Config.WorldStationLocation);
				BeamComp->SetVariableLinearColor(FName(TEXT("User.BeamColor")), Config.StreamColor);
				StationBeamComponents.Add(Config.ChannelName, BeamComp);
			}
		}
	}

	Super::BeginPlay();

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
	const FVector CenterLoc = GetActorLocation();

	// Saxion Pit floor dimensions: 665.2cm (X) x 415.2cm (Y). Stations are positioned along the perimeter railing.
	const float HalfW = 332.6f;
	const float HalfH = 207.6f;

	for (int32 Index = 0; Index < 20; ++Index)
	{
		FStationVoiceConfig Config;
		Config.ChannelName = FName(StationTable[Index].Name);
		Config.BaseFrequencyHz = 261.63f; // Set dynamically by ApplyHarmonicScaleFrequencies
		Config.TimbreProfile = StationTable[Index].Timbre;
		Config.StreamColor = StationTable[Index].Color;

		float StationX = 0.0f;
		float StationY = 0.0f;

		// 20 stations distributed along the 4 edges of the Pit railing:
		// South railing: 7 stations (Index 0 to 6)
		if (Index < 7)
		{
			const float Ratio = static_cast<float>(Index) / 6.0f;
			StationX = FMath::Lerp(-HalfW, HalfW, Ratio);
			StationY = -HalfH;
		}
		// East railing: 3 stations (Index 7 to 9)
		else if (Index < 10)
		{
			const float Ratio = static_cast<float>(Index - 6) / 4.0f;
			StationX = HalfW;
			StationY = FMath::Lerp(-HalfH, HalfH, Ratio);
		}
		// North railing: 7 stations (Index 10 to 16)
		else if (Index < 17)
		{
			const float Ratio = static_cast<float>(Index - 10) / 6.0f;
			StationX = FMath::Lerp(HalfW, -HalfW, Ratio);
			StationY = HalfH;
		}
		// West railing: 3 stations (Index 17 to 19)
		else
		{
			const float Ratio = static_cast<float>(Index - 16) / 4.0f;
			StationX = -HalfW;
			StationY = FMath::Lerp(HalfH, -HalfH, Ratio);
		}

		Config.WorldStationLocation = CenterLoc + FVector(StationX, StationY, 0.0f);
		Config.PanPosition = FMath::Clamp(StationX / HalfW, -1.0f, 1.0f);

		StationConfigs.Add(Config);
	}
}

void AHarmonicConvergenceAudioManager::SetHarmonicScale(EHarmonicScaleMode NewScaleMode)
{
	ScaleMode = NewScaleMode;
	ApplyHarmonicScaleFrequencies();
}

void AHarmonicConvergenceAudioManager::ApplyHarmonicScaleFrequencies()
{
	// 20-Note Open Harmonic voicings across multiple octaves (C2 to C7)
	const float OpenMajorFreqs[20] = {
		65.41f, 196.00f, 293.66f, 783.99f, 130.81f, 329.63f, 987.77f, 392.00f, 1174.66f, 98.00f,
		440.00f, 1318.51f, 523.25f, 587.33f, 1567.98f, 196.00f, 659.25f, 2093.00f, 261.63f, 1046.50f
	};

	const float OpenLydianFreqs[20] = {
		65.41f, 196.00f, 293.66f, 739.99f, 130.81f, 329.63f, 987.77f, 392.00f, 1174.66f, 98.00f,
		440.00f, 1479.98f, 523.25f, 587.33f, 1760.00f, 196.00f, 659.25f, 2093.00f, 261.63f, 1046.50f
	};

	const float OpenMinorFreqs[20] = {
		65.41f, 196.00f, 311.13f, 783.99f, 130.81f, 349.23f, 932.33f, 392.00f, 1244.51f, 98.00f,
		466.16f, 1396.91f, 523.25f, 622.25f, 1567.98f, 196.00f, 698.46f, 2093.00f, 261.63f, 1046.50f
	};

	const float OpenHirajoshiFreqs[20] = {
		65.41f, 196.00f, 277.18f, 783.99f, 130.81f, 349.23f, 880.00f, 392.00f, 1108.73f, 98.00f,
		415.30f, 1396.91f, 523.25f, 554.37f, 1567.98f, 196.00f, 698.46f, 2093.00f, 261.63f, 1046.50f
	};

	const float OpenDorianFreqs[20] = {
		65.41f, 196.00f, 293.66f, 783.99f, 130.81f, 329.63f, 932.33f, 392.00f, 1174.66f, 98.00f,
		440.00f, 1318.51f, 523.25f, 587.33f, 1567.98f, 196.00f, 659.25f, 2093.00f, 261.63f, 1046.50f
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

		// 2. Activate per-station Niagara beam component directly in C++
		if (TObjectPtr<UNiagaraComponent>* BeamCompPtr = StationBeamComponents.Find(StationChannelName))
		{
			if (UNiagaraComponent* BeamComp = BeamCompPtr->Get())
			{
				BeamComp->Activate(true);
				if (FoundVoiceIndex != INDEX_NONE)
				{
					BeamComp->SetVariableVec3(FName(TEXT("User.CenterLocation")), GetActorLocation());
					BeamComp->SetVariableVec3(FName(TEXT("User.BeamStartLocation")), StationConfigs[FoundVoiceIndex].WorldStationLocation);
					BeamComp->SetVariableLinearColor(FName(TEXT("User.BeamColor")), StationConfigs[FoundVoiceIndex].StreamColor);
				}
			}
		}

		// 3. Broadcast high-level station event for Niagara / Blueprints
		OnStationVoiceStarted.Broadcast(StationChannelName, VoiceState->CurrentFrequency);

		// 4. Optional MetaSound Trigger
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

			// 2. Deactivate per-station Niagara beam component directly in C++
			if (TObjectPtr<UNiagaraComponent>* BeamCompPtr = StationBeamComponents.Find(StationChannelName))
			{
				if (UNiagaraComponent* BeamComp = BeamCompPtr->Get())
				{
					BeamComp->Deactivate();
				}
			}

			// 3. Broadcast high-level station event for Niagara / Blueprints
			OnStationVoiceStopped.Broadcast(StationChannelName);

			// 4. Optional MetaSound Trigger
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

bool AHarmonicConvergenceAudioManager::GetStationConfig(FName StationChannelName, FStationVoiceConfig& OutConfig) const
{
	for (const FStationVoiceConfig& Config : StationConfigs)
	{
		if (Config.ChannelName == StationChannelName)
		{
			OutConfig = Config;
			return true;
		}
	}
	return false;
}

void AHarmonicConvergenceAudioManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateConvergenceParameters(DeltaTime);
	UpdateBeatSlots(DeltaTime);
	UpdateNiagaraParameters(DeltaTime);
	UpdateAttractMode(DeltaTime);
	PushMetaSoundParameters();
}

void AHarmonicConvergenceAudioManager::UpdateNiagaraParameters(float DeltaTime)
{
	const float CoreBrightness = FMath::Pow(ConvergenceEnergy, 1.5f);
	for (const TPair<FName, TObjectPtr<UNiagaraComponent>>& Pair : StationBeamComponents)
	{
		if (UNiagaraComponent* BeamComp = Pair.Value.Get())
		{
			if (BeamComp->IsActive())
			{
				float StationModulation = 0.0f;
				if (const FVoiceRuntimeState* VoiceState = VoiceStates.Find(Pair.Key))
				{
					StationModulation = VoiceState->ModulationIntensity;
				}
				BeamComp->SetVariableFloat(FName(TEXT("User.ConvergenceEnergy")), ConvergenceEnergy);
				BeamComp->SetVariableInt(FName(TEXT("User.ActiveStationCount")), ActiveVoiceCount);
				BeamComp->SetVariableFloat(FName(TEXT("User.CoreBrightness")), CoreBrightness);
				BeamComp->SetVariableFloat(FName(TEXT("User.VoiceModulation")), StationModulation);
			}
		}
	}

	if (CenterVortexComponent)
	{
		FLinearColor TargetCoreColor(0.35f, 0.8f, 1.0f, 1.0f);
		if (ActiveVoiceCount > 0)
		{
			FLinearColor ColorSum(0.0f, 0.0f, 0.0f, 0.0f);
			int32 ActiveColorCount = 0;
			for (const FStationVoiceConfig& Config : StationConfigs)
			{
				if (const FVoiceRuntimeState* VoiceState = VoiceStates.Find(Config.ChannelName))
				{
					if (VoiceState->bIsPressed)
					{
						ColorSum += Config.StreamColor;
						ActiveColorCount++;
					}
				}
			}
			if (ActiveColorCount > 0)
			{
				TargetCoreColor = ColorSum / static_cast<float>(ActiveColorCount);
				TargetCoreColor.A = 1.0f;
			}
		}

		CurrentCoreColor = FMath::CInterpTo(CurrentCoreColor, TargetCoreColor, DeltaTime, 5.0f);
		CenterVortexComponent->SetVariableFloat(FName(TEXT("User.ConvergenceEnergy")), ConvergenceEnergy);
		CenterVortexComponent->SetVariableInt(FName(TEXT("User.ActiveStationCount")), ActiveVoiceCount);
		CenterVortexComponent->SetVariableFloat(FName(TEXT("User.CoreBrightness")), CoreBrightness);
		CenterVortexComponent->SetVariableLinearColor(FName(TEXT("User.CoreColor")), CurrentCoreColor);
	}
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

				// Check if hold duration qualifies for unlocking a persistent looping pulse beat
				if (VoiceState->CurrentHoldDuration >= HoldDurationToUnlockBeat)
				{
					int32 ExistingSlotIdx = INDEX_NONE;
					for (int32 SlotIdx = 0; SlotIdx < PersistentBeatSlots.Num(); ++SlotIdx)
					{
						if (PersistentBeatSlots[SlotIdx].bIsActive && PersistentBeatSlots[SlotIdx].StationChannelName == StationChannelName)
						{
							ExistingSlotIdx = SlotIdx;
							break;
						}
					}

					if (ExistingSlotIdx != INDEX_NONE)
					{
						// Still holding: keep beat lifespan replenished
						PersistentBeatSlots[ExistingSlotIdx].RemainingLifespan = BeatLifespanSeconds;
					}
					else
					{
						// Find an open beat slot, or evict the one closest to expiry
						int32 TargetSlotIdx = INDEX_NONE;
						for (int32 SlotIdx = 0; SlotIdx < PersistentBeatSlots.Num(); ++SlotIdx)
						{
							if (!PersistentBeatSlots[SlotIdx].bIsActive)
							{
								TargetSlotIdx = SlotIdx;
								break;
							}
						}

						if (TargetSlotIdx == INDEX_NONE)
						{
							float LowestLifespan = TNumericLimits<float>::Max();
							for (int32 SlotIdx = 0; SlotIdx < PersistentBeatSlots.Num(); ++SlotIdx)
							{
								if (PersistentBeatSlots[SlotIdx].RemainingLifespan < LowestLifespan)
								{
									LowestLifespan = PersistentBeatSlots[SlotIdx].RemainingLifespan;
									TargetSlotIdx = SlotIdx;
								}
							}
						}

						if (TargetSlotIdx != INDEX_NONE)
						{
							FPersistentBeatSlotInfo& Slot = PersistentBeatSlots[TargetSlotIdx];
							Slot.bIsActive = true;
							Slot.StationChannelName = StationChannelName;
							Slot.RemainingLifespan = BeatLifespanSeconds;
							Slot.BaseFrequencyHz = StationConfigs[StationIndex].BaseFrequencyHz;
							Slot.TimbreProfile = StationConfigs[StationIndex].TimbreProfile;
							Slot.PanPosition = StationConfigs[StationIndex].PanPosition;

							if (ProceduralSynthComponent)
							{
								ProceduralSynthComponent->SetRhythmicPulseSlot(
									TargetSlotIdx,
									true,
									Slot.AssignedBeatStep,
									Slot.BaseFrequencyHz,
									Slot.PanPosition,
									Slot.TimbreProfile,
									1.0f);
							}

							OnPersistentBeatActivated.Broadcast(StationChannelName, Slot.AssignedBeatStep, Slot.BaseFrequencyHz);
						}
					}
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

void AHarmonicConvergenceAudioManager::UpdateBeatSlots(float DeltaTime)
{
	for (int32 SlotIdx = 0; SlotIdx < PersistentBeatSlots.Num(); ++SlotIdx)
	{
		FPersistentBeatSlotInfo& Slot = PersistentBeatSlots[SlotIdx];
		if (!Slot.bIsActive)
		{
			continue;
		}

		// When the player releases the button, count down its persistent lifespan
		bool bStationStillPressed = false;
		if (const FVoiceRuntimeState* VoiceState = VoiceStates.Find(Slot.StationChannelName))
		{
			bStationStillPressed = VoiceState->bIsPressed;
		}

		if (!bStationStillPressed)
		{
			Slot.RemainingLifespan -= DeltaTime;
			if (Slot.RemainingLifespan <= 0.0f)
			{
				Slot.bIsActive = false;
				Slot.StationChannelName = NAME_None;
				Slot.RemainingLifespan = 0.0f;

				if (ProceduralSynthComponent)
				{
					ProceduralSynthComponent->SetRhythmicPulseSlot(SlotIdx, false);
				}

				OnPersistentBeatDeactivated.Broadcast(SlotIdx);
				continue;
			}
		}

		// Smooth natural fade out in the final 5 seconds of lifespan
		const float FadeGain = (Slot.RemainingLifespan < 5.0f) ? FMath::Clamp(Slot.RemainingLifespan / 5.0f, 0.0f, 1.0f) : 1.0f;
		if (ProceduralSynthComponent)
		{
			ProceduralSynthComponent->SetRhythmicPulseSlot(
				SlotIdx,
				true,
				Slot.AssignedBeatStep,
				Slot.BaseFrequencyHz,
				Slot.PanPosition,
				Slot.TimbreProfile,
				FadeGain);
		}
	}
}

void AHarmonicConvergenceAudioManager::ClearAllBeatSlots()
{
	for (int32 SlotIdx = 0; SlotIdx < PersistentBeatSlots.Num(); ++SlotIdx)
	{
		PersistentBeatSlots[SlotIdx].bIsActive = false;
		PersistentBeatSlots[SlotIdx].StationChannelName = NAME_None;
		PersistentBeatSlots[SlotIdx].RemainingLifespan = BeatLifespanSeconds;
	}

	if (ProceduralSynthComponent)
	{
		ProceduralSynthComponent->ClearAllRhythmicPulseSlots();
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
