#include "HarmonicConvergenceAudioManager.h"
#include "HarmonicConvergenceSynthComponent.h"
#include "InputManagerSubSystem.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
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

	static ConstructorHelpers::FObjectFinderOptional<UNiagaraSystem> DefaultCrystalSystem(TEXT("/Game/HarmonicConvergence/NS_BeatCrystal.NS_BeatCrystal"));
	if (DefaultCrystalSystem.Succeeded())
	{
		BeatCrystalSystem = DefaultCrystalSystem.Get();
	}
}

int32 AHarmonicConvergenceAudioManager::GetStationSection(int32 StationIndex) const
{
	// Maps the 4 railing sides of the Saxion Pit to 4 Incredibox instrument families:
	if (StationIndex < 7)  return 0; // South (Stations 0–6):  Beats / Sub-Bass (Beat 0)
	if (StationIndex < 10) return 1; // East  (Stations 7–9):  Acoustic Percussion / Ticks (Beat 2)
	if (StationIndex < 17) return 2; // North (Stations 10–16): Harmonic Warm Pads (Beat 1)
	return 3;                        // West  (Stations 17–19): Melodic Leads / Chimes (Beat 3)
}

int32 AHarmonicConvergenceAudioManager::GetCurrentChordProgressionIndex() const
{
	return ProceduralSynthComponent ? ProceduralSynthComponent->GetCurrentChordIndex() : 0;
}

FVector AHarmonicConvergenceAudioManager::GetStationCrystalWorldLocation(int32 StationIndex) const
{
	const FVector CenterLoc = GetActorLocation();
	if (!StationConfigs.IsValidIndex(StationIndex))
	{
		return CenterLoc;
	}

	// Calculate radial vector from center to the station on the floor XY plane
	const FVector RadialDir = (StationConfigs[StationIndex].WorldStationLocation - CenterLoc).GetSafeNormal2D();
	FVector CrystalPos = CenterLoc + RadialDir * CentralCrystalRingRadius;
	CrystalPos.Z = CenterLoc.Z;
	return CrystalPos;
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

	// Initialize 20 dedicated Persistent Beat Slots (one for each station / drum pad)
	PersistentBeatSlots.SetNum(StationConfigs.Num());
	for (int32 StationIdx = 0; StationIdx < StationConfigs.Num(); ++StationIdx)
	{
		PersistentBeatSlots[StationIdx].SlotIndex = StationIdx;
		PersistentBeatSlots[StationIdx].RhythmicPatternMask = StationConfigs[StationIdx].RhythmicPatternMask;
		PersistentBeatSlots[StationIdx].bIsActive = false;
		PersistentBeatSlots[StationIdx].StationIndex = StationIdx;
		PersistentBeatSlots[StationIdx].RemainingLifespan = BeatLifespanSeconds;
	}

	// Initialize Stem Audio Components for optional pre-rendered WAV SoundWave stems
	StationStemAudioComponents.SetNum(StationConfigs.Num());
	for (int32 StationIdx = 0; StationIdx < StationConfigs.Num(); ++StationIdx)
	{
		StationStemAudioComponents[StationIdx] = nullptr;
		if (StationConfigs[StationIdx].AudioStemAsset)
		{
			UAudioComponent* StemComp = NewObject<UAudioComponent>(this);
			StemComp->SetSound(StationConfigs[StationIdx].AudioStemAsset);
			StemComp->bAutoActivate = false;
			StemComp->bIsUISound = false;
			StemComp->SetVolumeMultiplier(0.0f);
			StemComp->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
			StemComp->RegisterComponent();
			StemComp->Play();
			StationStemAudioComponents[StationIdx] = StemComp;
		}
	}

	// Ensure any legacy attached Niagara components on child Blueprints are deactivated
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

	// 1. Spawn the 20 Crystal Shards forming the Central Circular Core (Always in the center, replacing the vortex)
	StationCrystalComponents.SetNum(StationConfigs.Num());
	StationCrystalMeshComponents.SetNum(StationConfigs.Num());
	StationCrystalDynamicMaterials.SetNum(StationConfigs.Num());

	for (int32 StationIdx = 0; StationIdx < StationConfigs.Num(); ++StationIdx)
	{
		StationCrystalComponents[StationIdx] = nullptr;
		StationCrystalMeshComponents[StationIdx] = nullptr;
		StationCrystalDynamicMaterials[StationIdx] = nullptr;
		StationPulseFlash[StationIdx] = 0.0f;
		StationBeatPulse[StationIdx] = 0.0f;
		StationChargeProgress[StationIdx] = 0.0f;

		const FVector CrystalWorldLoc = GetStationCrystalWorldLocation(StationIdx);
		const FVector RadialDir = (StationConfigs[StationIdx].WorldStationLocation - CenterLoc).GetSafeNormal2D();
		FRotator CrystalRot = RadialDir.Rotation();
		CrystalRot.Pitch = 12.0f; // Upright with slight outward flare

		// Optional Physical Static Mesh support for future custom 3D crystal assets
		if (CrystalMeshAsset)
		{
			UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(this);
			MeshComp->SetStaticMesh(CrystalMeshAsset);
			MeshComp->SetWorldLocationAndRotation(CrystalWorldLoc, CrystalRot);
			MeshComp->SetWorldScale3D(CrystalMeshScale);
			MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			MeshComp->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
			MeshComp->RegisterComponent();

			if (CrystalBaseMaterial)
			{
				UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(CrystalBaseMaterial, this);
				DynMat->SetVectorParameterValue(TEXT("EmissiveColor"), StationConfigs[StationIdx].StreamColor);
				DynMat->SetScalarParameterValue(TEXT("Brightness"), 0.15f);
				MeshComp->SetMaterial(0, DynMat);
				StationCrystalDynamicMaterials[StationIdx] = DynMat;
			}
			StationCrystalMeshComponents[StationIdx] = MeshComp;
		}

		// Niagara Crystal Core & Floor Pulse Ring Emitter
		if (BeatCrystalSystem)
		{
			if (UNiagaraComponent* CrystalComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				GetWorld(), BeatCrystalSystem, CrystalWorldLoc, CrystalRot, FVector(1.0f), false, true))
			{
				CrystalComp->SetVariableLinearColor(FName(TEXT("User.CrystalColor")), StationConfigs[StationIdx].StreamColor);
				CrystalComp->SetVariableFloat(FName(TEXT("User.LifespanRatio")), 0.15f); // Serene dormant ambient quartz glow
				CrystalComp->SetVariableFloat(FName(TEXT("User.PulseFlash")), 0.0f);
				CrystalComp->SetVariableFloat(FName(TEXT("User.BeatPulse")), 0.0f);
				StationCrystalComponents[StationIdx] = CrystalComp;
			}
		}
	}

	// 2. Spawn per-station Beam Systems aimed directly at their corresponding crystal in the core
	for (int32 StationIdx = 0; StationIdx < StationConfigs.Num(); ++StationIdx)
	{
		const FStationVoiceConfig& Config = StationConfigs[StationIdx];
		FVoiceRuntimeState State;
		State.CurrentFrequency = Config.BaseFrequencyHz;
		VoiceStates.Add(Config.ChannelName, State);

		if (StationBeamSystem)
		{
			if (UNiagaraComponent* BeamComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				GetWorld(), StationBeamSystem, CenterLoc, FRotator::ZeroRotator, FVector(1.0f), false, false))
			{
				const FVector CrystalTargetLoc = GetStationCrystalWorldLocation(StationIdx);
				BeamComp->SetVariableVec3(FName(TEXT("User.CenterLocation")), CrystalTargetLoc);
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
		int32 PatternMask; // 16-step rhythmic pattern bitmask (bits 0..15 corresponding to 16th-note steps in the 4/4 bar)
	};

	const FStationInitData StationTable[20] = {
		// South Railing (Stations 0–6): Beats / Sub-Bass
		{ TEXT("player1"),  EVoiceTimbreProfile::SubBassPad,        FLinearColor(0.0f, 0.9f, 1.0f), 0x1111 }, // Four-on-the-floor Kick (steps 0, 4, 8, 12)
		{ TEXT("player2"),  EVoiceTimbreProfile::SubBassPad,        FLinearColor(0.0f, 0.6f, 1.0f), 0x0449 }, // Breakbeat / Funk Kick (steps 0, 3, 6, 10)
		{ TEXT("player3"),  EVoiceTimbreProfile::SubBassPad,        FLinearColor(0.1f, 0.4f, 1.0f), 0x0101 }, // 808 Sub Drop (steps 0, 8)
		{ TEXT("player4"),  EVoiceTimbreProfile::SubBassPad,        FLinearColor(0.0f, 0.8f, 0.9f), 0x4444 }, // Offbeat Bouncy Sub (steps 2, 6, 10, 14)
		{ TEXT("player5"),  EVoiceTimbreProfile::SubBassPad,        FLinearColor(0.2f, 0.5f, 1.0f), 0x0481 }, // Hip-Hop Kick (steps 0, 7, 10)
		{ TEXT("player6"),  EVoiceTimbreProfile::SubBassPad,        FLinearColor(0.0f, 1.0f, 0.8f), 0x2041 }, // UK Garage Skip Kick (steps 0, 6, 13)
		{ TEXT("player7"),  EVoiceTimbreProfile::SubBassPad,        FLinearColor(0.3f, 0.2f, 0.9f), 0x0905 }, // Driving Double-Tap Sub (steps 0, 2, 8, 11)

		// East Railing (Stations 7–9): Acoustic Percussion / Snare / Shaker
		{ TEXT("player8"),  EVoiceTimbreProfile::CrystallineChime,  FLinearColor(1.0f, 0.6f, 0.0f), 0x1010 }, // Snare / Clap Backbeat (steps 4, 12)
		{ TEXT("player9"),  EVoiceTimbreProfile::CrystallineChime,  FLinearColor(0.9f, 0.8f, 0.1f), 0x5555 }, // Rolling 8th-Note Hi-Hats (steps 0, 2, 4, 6, 8, 10, 12, 14)
		{ TEXT("player10"), EVoiceTimbreProfile::CrystallineChime,  FLinearColor(1.0f, 0.45f, 0.1f), 0xAAAA }, // Shaker 16th Syncopation (steps 1, 3, 5, 7, 9, 11, 13, 15)

		// North Railing (Stations 10–16): Harmonic Warm Pads / Chords
		{ TEXT("player11"), EVoiceTimbreProfile::WarmPad,           FLinearColor(0.9f, 0.1f, 0.5f), 0x4444 }, // Offbeat Reggae/Funk Chords (steps 2, 6, 10, 14)
		{ TEXT("player12"), EVoiceTimbreProfile::WarmPad,           FLinearColor(1.0f, 0.2f, 0.3f), 0x0001 }, // Ambient Swell Bed (step 0)
		{ TEXT("player13"), EVoiceTimbreProfile::WarmPad,           FLinearColor(0.8f, 0.1f, 0.8f), 0x4108 }, // Neo-Soul Chord Stabs (steps 3, 8, 14)
		{ TEXT("player14"), EVoiceTimbreProfile::WarmPad,           FLinearColor(0.6f, 0.1f, 0.9f), 0x0410 }, // Rhythmic Fifth Swell (steps 4, 10)
		{ TEXT("player15"), EVoiceTimbreProfile::WarmPad,           FLinearColor(1.0f, 0.3f, 0.6f), 0x1084 }, // Warm Staccato Rhythm (steps 2, 7, 12)
		{ TEXT("player16"), EVoiceTimbreProfile::WarmPad,           FLinearColor(0.7f, 0.2f, 0.7f), 0x4040 }, // High Voice Shimmer (steps 6, 14)
		{ TEXT("player17"), EVoiceTimbreProfile::WarmPad,           FLinearColor(0.9f, 0.4f, 0.2f), 0x0101 }, // Root Bed Anchor (steps 0, 8)

		// West Railing (Stations 17–19): Melodic Leads & Crystalline Chimes
		{ TEXT("player18"), EVoiceTimbreProfile::VortexSweep,       FLinearColor(0.1f, 1.0f, 0.4f), 0x0A28 }, // Funky Melodic Lead (steps 3, 5, 9, 11)
		{ TEXT("player19"), EVoiceTimbreProfile::CrystallineChime,  FLinearColor(0.3f, 1.0f, 0.7f), 0x2492 }, // Glass Kalimba Arpeggio (steps 1, 4, 7, 10, 13)
		{ TEXT("player20"), EVoiceTimbreProfile::VortexSweep,       FLinearColor(1.0f, 1.0f, 1.0f), 0xD000 }  // Turnaround Swoosh Flourish (steps 12, 14, 15)
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
		Config.RhythmicPatternMask = StationTable[Index].PatternMask;

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

		// 2. Activate per-station Niagara beam component aimed directly at its crystal in the core
		if (TObjectPtr<UNiagaraComponent>* BeamCompPtr = StationBeamComponents.Find(StationChannelName))
		{
			if (UNiagaraComponent* BeamComp = BeamCompPtr->Get())
			{
				BeamComp->Activate(true);
				if (FoundVoiceIndex != INDEX_NONE)
				{
					const FVector CrystalTargetLoc = GetStationCrystalWorldLocation(FoundVoiceIndex);
					BeamComp->SetVariableVec3(FName(TEXT("User.CenterLocation")), CrystalTargetLoc);
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

			// If released before reaching full activation, the crystal will smoothly decay back to dormant
			// (Handled automatically in UpdateConvergenceParameters)

			// 1. Direct Pure C++ Procedural Synth Trigger
			if (ProceduralSynthComponent && FoundVoiceIndex != INDEX_NONE)
			{
				ProceduralSynthComponent->NoteOff(FoundVoiceIndex);
			}

			// 2. Deactivate per-station Niagara beam component
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

	// Decay pulse flashes and floor shockwave pulses back to zero every frame
	for (int32 StIdx = 0; StIdx < StationConfigs.Num(); ++StIdx)
	{
		if (StationPulseFlash[StIdx] > 0.001f)
		{
			StationPulseFlash[StIdx] = FMath::FInterpTo(StationPulseFlash[StIdx], 0.0f, DeltaTime, 10.0f);
			if (StationCrystalComponents.IsValidIndex(StIdx) && StationCrystalComponents[StIdx])
			{
				StationCrystalComponents[StIdx]->SetVariableFloat(FName(TEXT("User.PulseFlash")), StationPulseFlash[StIdx]);
			}
			if (StationCrystalDynamicMaterials.IsValidIndex(StIdx) && StationCrystalDynamicMaterials[StIdx])
			{
				StationCrystalDynamicMaterials[StIdx]->SetScalarParameterValue(TEXT("PulseFlash"), StationPulseFlash[StIdx]);
			}
		}

		if (StationBeatPulse[StIdx] > 0.001f)
		{
			StationBeatPulse[StIdx] = FMath::FInterpTo(StationBeatPulse[StIdx], 0.0f, DeltaTime, 12.0f);
			if (StationCrystalComponents.IsValidIndex(StIdx) && StationCrystalComponents[StIdx])
			{
				StationCrystalComponents[StIdx]->SetVariableFloat(FName(TEXT("User.BeatPulse")), StationBeatPulse[StIdx]);
			}
		}
	}

	UpdateConvergenceParameters(DeltaTime);
	UpdateBeatSlots(DeltaTime);
	UpdateNiagaraParameters(DeltaTime);
	UpdateAttractMode(DeltaTime);
	PushMetaSoundParameters();

	// Master 16-Step Beat Synchronization: pulse crystals whose pattern contains the current 16th-note step
	if (ProceduralSynthComponent)
	{
		const int32 CurrentStep = ProceduralSynthComponent->GetCurrentBeatStep();
		if (CurrentStep != LastBeatStep)
		{
			LastBeatStep = CurrentStep;
			const int32 StepBit = (1 << CurrentStep);

			for (int32 SlotIdx = 0; SlotIdx < PersistentBeatSlots.Num(); ++SlotIdx)
			{
				const FPersistentBeatSlotInfo& Slot = PersistentBeatSlots[SlotIdx];
				if (Slot.bIsActive && StationConfigs.IsValidIndex(Slot.StationIndex))
				{
					const int32 StIdx = Slot.StationIndex;
					if (Slot.RhythmicPatternMask & StepBit)
					{
						StationPulseFlash[StIdx] = 1.0f;
						StationBeatPulse[StIdx] = 1.0f;
						if (StationCrystalComponents.IsValidIndex(StIdx) && StationCrystalComponents[StIdx])
						{
							StationCrystalComponents[StIdx]->SetVariableFloat(FName(TEXT("User.PulseFlash")), 1.0f);
							StationCrystalComponents[StIdx]->SetVariableFloat(FName(TEXT("User.BeatPulse")), 1.0f);
						}
					if (StationCrystalDynamicMaterials.IsValidIndex(StIdx) && StationCrystalDynamicMaterials[StIdx])
					{
						StationCrystalDynamicMaterials[StIdx]->SetScalarParameterValue(TEXT("PulseFlash"), 1.0f);
					}
					}
				}
			}
		}
	}
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

				// Charging the crystal: ChargeProgress builds from 0.0 to 1.0 over HoldDurationToUnlockBeat
				StationChargeProgress[StationIndex] = FMath::Clamp(VoiceState->CurrentHoldDuration / HoldDurationToUnlockBeat, 0.0f, 1.0f);

				// Visual brightness scales up from dormant quartz (0.15) to brilliant radiant active (1.0)
				const float DynamicBrightness = FMath::Lerp(0.15f, 1.0f, StationChargeProgress[StationIndex]);
				if (StationCrystalComponents.IsValidIndex(StationIndex) && StationCrystalComponents[StationIndex])
				{
					StationCrystalComponents[StationIndex]->SetVariableFloat(FName(TEXT("User.LifespanRatio")), DynamicBrightness);
					// Charging crackle shimmer
					StationCrystalComponents[StationIndex]->SetVariableFloat(FName(TEXT("User.PulseFlash")), StationChargeProgress[StationIndex] * 0.35f);
				}
				if (StationCrystalDynamicMaterials.IsValidIndex(StationIndex) && StationCrystalDynamicMaterials[StationIndex])
				{
					StationCrystalDynamicMaterials[StationIndex]->SetScalarParameterValue(TEXT("Brightness"), DynamicBrightness);
				}

				// Audibly swells louder, opens low-pass filter and blooms harmonics as energy pours into the crystal
				VoiceState->ModulationIntensity = StationChargeProgress[StationIndex];
				TotalModulationIntensityAccumulator += VoiceState->ModulationIntensity;

				if (ProceduralSynthComponent)
				{
					ProceduralSynthComponent->SetVoiceModulation(StationIndex, VoiceState->ModulationIntensity);
				}

				// Check if crystal is fully charged to activate and lock in persistent beat looper (Drum Pad Mode)
				if (VoiceState->CurrentHoldDuration >= HoldDurationToUnlockBeat)
				{
					const int32 SlotIdx = StationIndex;
					if (PersistentBeatSlots.IsValidIndex(SlotIdx))
					{
						FPersistentBeatSlotInfo& Slot = PersistentBeatSlots[SlotIdx];

						if (Slot.bIsActive)
						{
							// Still holding: keep beat lifespan topped up
							Slot.RemainingLifespan = BeatLifespanSeconds;
						}
						else
						{
							// Count active crystals against MaxActiveCrystals
							int32 CurrentlyActiveCrystals = 0;
							for (const FPersistentBeatSlotInfo& S : PersistentBeatSlots)
							{
								if (S.bIsActive) CurrentlyActiveCrystals++;
							}

							if (CurrentlyActiveCrystals < MaxActiveCrystals)
							{
								Slot.bIsActive = true;
								Slot.StationChannelName = StationChannelName;
								Slot.StationIndex = StationIndex;
								Slot.RemainingLifespan = BeatLifespanSeconds;
								Slot.BaseFrequencyHz = StationConfigs[StationIndex].BaseFrequencyHz;
								Slot.TimbreProfile = StationConfigs[StationIndex].TimbreProfile;
								Slot.PanPosition = StationConfigs[StationIndex].PanPosition;
								Slot.RhythmicPatternMask = StationConfigs[StationIndex].RhythmicPatternMask;

								// If this station has an AudioStemAsset assigned, stem audio component handles audio;
								// otherwise, procedural 16-step synth slot is activated
								const bool bHasAudioStem = StationConfigs[StationIndex].AudioStemAsset != nullptr;
								if (ProceduralSynthComponent && !bHasAudioStem)
								{
									ProceduralSynthComponent->SetRhythmicPulseSlot(
										SlotIdx,
										true,
										Slot.RhythmicPatternMask,
										Slot.BaseFrequencyHz,
										Slot.PanPosition,
										Slot.TimbreProfile,
										1.0f);
								}

								// Audio ignition sound on the charging beam
								if (ProceduralSynthComponent)
								{
									ProceduralSynthComponent->TriggerVoiceIgnition(StationIndex);
								}

								// Crystal Ignition Flash!
								StationPulseFlash[StationIndex] = 1.0f;
								StationBeatPulse[StationIndex] = 1.0f;
								if (StationCrystalComponents.IsValidIndex(StationIndex) && StationCrystalComponents[StationIndex])
								{
									StationCrystalComponents[StationIndex]->SetVariableFloat(FName(TEXT("User.PulseFlash")), 1.0f);
									StationCrystalComponents[StationIndex]->SetVariableFloat(FName(TEXT("User.BeatPulse")), 1.0f);
								}
								if (StationCrystalDynamicMaterials.IsValidIndex(StationIndex) && StationCrystalDynamicMaterials[StationIndex])
								{
									StationCrystalDynamicMaterials[StationIndex]->SetScalarParameterValue(TEXT("PulseFlash"), 1.0f);
								}

								OnPersistentBeatActivated.Broadcast(StationChannelName, Slot.RhythmicPatternMask, Slot.BaseFrequencyHz);
							}
						}
					}
				}
			}
			else
			{
				// Station button is NOT pressed:
				// If this station is NOT currently an active looper, decay its charge back to dormant quartz
				const bool bIsActiveLooper = PersistentBeatSlots.IsValidIndex(StationIndex) &&
				                             PersistentBeatSlots[StationIndex].bIsActive;

				if (VoiceState->bIsOvercharged)
				{
					VoiceState->bIsOvercharged = false;
					if (ProceduralSynthComponent)
					{
						ProceduralSynthComponent->SetSlotOvercharge(StationIndex, false);
					}
				}

				if (!bIsActiveLooper && StationChargeProgress[StationIndex] > 0.001f)
				{
					StationChargeProgress[StationIndex] = FMath::FInterpTo(StationChargeProgress[StationIndex], 0.0f, DeltaTime, 4.0f);
					const float DecayBrightness = FMath::Lerp(0.15f, 1.0f, StationChargeProgress[StationIndex]);
					if (StationCrystalComponents.IsValidIndex(StationIndex) && StationCrystalComponents[StationIndex])
					{
						StationCrystalComponents[StationIndex]->SetVariableFloat(FName(TEXT("User.LifespanRatio")), DecayBrightness);
					}
					if (StationCrystalDynamicMaterials.IsValidIndex(StationIndex) && StationCrystalDynamicMaterials[StationIndex])
					{
						StationCrystalDynamicMaterials[StationIndex]->SetScalarParameterValue(TEXT("Brightness"), DecayBrightness);
					}
				}
			}
		}
	}

	const int32 TotalConfiguredStations = FMath::Max(1, StationConfigs.Num());
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
		const int32 StIdx = Slot.StationIndex;
		const bool bValidStation = StationConfigs.IsValidIndex(StIdx);

		if (!Slot.bIsActive || !bValidStation)
		{
			// Smoothly fade out stem audio component if not active
			if (StationStemAudioComponents.IsValidIndex(SlotIdx) && StationStemAudioComponents[SlotIdx])
			{
				const float CurrentVol = StationStemAudioComponents[SlotIdx]->VolumeMultiplier;
				if (CurrentVol > 0.001f)
				{
					StationStemAudioComponents[SlotIdx]->SetVolumeMultiplier(FMath::FInterpTo(CurrentVol, 0.0f, DeltaTime, 6.0f));
				}
			}
			continue;
		}

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
				StationChargeProgress[StIdx] = 0.0f;

				// Echo Ripple on crystal expiration: glass bell chime + neighbor ring energy transfer
				if (ProceduralSynthComponent)
				{
					ProceduralSynthComponent->TriggerEchoRipple(Slot.BaseFrequencyHz * 2.0f, Slot.PanPosition);
					ProceduralSynthComponent->SetRhythmicPulseSlot(SlotIdx, false);
					ProceduralSynthComponent->SetSlotOvercharge(SlotIdx, false);
				}

				// Energy transfer: spark neighbor crystals with a soft ghost echo pulse
				const int32 TotalStCount = StationConfigs.Num();
				if (TotalStCount > 1)
				{
					const int32 LeftNeighbor = (StIdx + TotalStCount - 1) % TotalStCount;
					const int32 RightNeighbor = (StIdx + 1) % TotalStCount;
					if (PersistentBeatSlots.IsValidIndex(LeftNeighbor) && !PersistentBeatSlots[LeftNeighbor].bIsActive)
					{
						StationPulseFlash[LeftNeighbor] = 0.55f;
					}
					if (PersistentBeatSlots.IsValidIndex(RightNeighbor) && !PersistentBeatSlots[RightNeighbor].bIsActive)
					{
						StationPulseFlash[RightNeighbor] = 0.55f;
					}
				}

				if (StationStemAudioComponents.IsValidIndex(SlotIdx) && StationStemAudioComponents[SlotIdx])
				{
					StationStemAudioComponents[SlotIdx]->SetVolumeMultiplier(0.0f);
				}

				if (StationCrystalComponents.IsValidIndex(StIdx) && StationCrystalComponents[StIdx])
				{
					StationCrystalComponents[StIdx]->SetVariableFloat(FName(TEXT("User.LifespanRatio")), 0.15f);
					StationCrystalComponents[StIdx]->SetVariableFloat(FName(TEXT("User.PulseFlash")), 0.0f);
					StationCrystalComponents[StIdx]->SetVariableFloat(FName(TEXT("User.EchoRipple")), 1.0f);
				}
				if (StationCrystalDynamicMaterials.IsValidIndex(StIdx) && StationCrystalDynamicMaterials[StIdx])
				{
					StationCrystalDynamicMaterials[StIdx]->SetScalarParameterValue(TEXT("Brightness"), 0.15f);
					StationCrystalDynamicMaterials[StIdx]->SetScalarParameterValue(TEXT("PulseFlash"), 0.0f);
				}

				OnCrystalEchoRippleTriggered.Broadcast(StIdx, GetStationCrystalWorldLocation(StIdx));
				OnPersistentBeatDeactivated.Broadcast(SlotIdx);
				continue;
			}
		}

		// Visual Battery Meter: continuously dims from 1.0 (radiant active) down to 0.15 (dormant quartz) as power drains
		const float LifespanFraction = (BeatLifespanSeconds > 0.0f) ? FMath::Clamp(Slot.RemainingLifespan / BeatLifespanSeconds, 0.0f, 1.0f) : 0.0f;
		const float VisualGlow = FMath::Lerp(0.15f, 1.0f, FMath::Pow(LifespanFraction, 0.75f));

		// Audio fade out in the final 6 seconds of lifespan
		const float AudioFadeWindow = 6.0f;
		const float AudioFadeAlpha = (Slot.RemainingLifespan < AudioFadeWindow) ? FMath::Clamp(Slot.RemainingLifespan / AudioFadeWindow, 0.0f, 1.0f) : 1.0f;

		// Synchronized Stem volume interpolation
		if (StationStemAudioComponents.IsValidIndex(SlotIdx) && StationStemAudioComponents[SlotIdx])
		{
			const float CurrentVol = StationStemAudioComponents[SlotIdx]->VolumeMultiplier;
			StationStemAudioComponents[SlotIdx]->SetVolumeMultiplier(FMath::FInterpTo(CurrentVol, AudioFadeAlpha, DeltaTime, 8.0f));
		}

		if (!bStationStillPressed)
		{
			if (StationCrystalComponents.IsValidIndex(StIdx) && StationCrystalComponents[StIdx])
			{
				StationCrystalComponents[StIdx]->SetVariableFloat(FName(TEXT("User.LifespanRatio")), VisualGlow);
			}
			if (StationCrystalDynamicMaterials.IsValidIndex(StIdx) && StationCrystalDynamicMaterials[StIdx])
			{
				StationCrystalDynamicMaterials[StIdx]->SetScalarParameterValue(TEXT("Brightness"), VisualGlow);
			}
		}

		// Update procedural volume scaling if using procedural synth
		const bool bHasAudioStem = StationConfigs[StIdx].AudioStemAsset != nullptr;
		if (ProceduralSynthComponent && !bHasAudioStem)
		{
			ProceduralSynthComponent->SetRhythmicPulseSlot(
				SlotIdx,
				true,
				Slot.RhythmicPatternMask,
				Slot.BaseFrequencyHz,
				Slot.PanPosition,
				Slot.TimbreProfile,
				AudioFadeAlpha);
		}
	}

	// Harmonic Overdrive Evaluation: track simultaneous active crystals for collective climax
	int32 CurrentlyActiveLoopers = 0;
	for (const FPersistentBeatSlotInfo& S : PersistentBeatSlots)
	{
		if (S.bIsActive) CurrentlyActiveLoopers++;
	}

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
}

void AHarmonicConvergenceAudioManager::ClearAllBeatSlots()
{
	for (int32 SlotIdx = 0; SlotIdx < PersistentBeatSlots.Num(); ++SlotIdx)
	{
		PersistentBeatSlots[SlotIdx].bIsActive = false;
		PersistentBeatSlots[SlotIdx].StationChannelName = NAME_None;
		PersistentBeatSlots[SlotIdx].StationIndex = INDEX_NONE;
		PersistentBeatSlots[SlotIdx].RemainingLifespan = BeatLifespanSeconds;

		if (StationStemAudioComponents.IsValidIndex(SlotIdx) && StationStemAudioComponents[SlotIdx])
		{
			StationStemAudioComponents[SlotIdx]->SetVolumeMultiplier(0.0f);
		}
	}

	if (ProceduralSynthComponent)
	{
		ProceduralSynthComponent->ClearAllRhythmicPulseSlots();
	}

	for (int32 StIdx = 0; StIdx < StationCrystalComponents.Num(); ++StIdx)
	{
		StationPulseFlash[StIdx] = 0.0f;
		StationBeatPulse[StIdx] = 0.0f;
		StationChargeProgress[StIdx] = 0.0f;
		if (StationCrystalComponents[StIdx])
		{
			StationCrystalComponents[StIdx]->SetVariableFloat(FName(TEXT("User.LifespanRatio")), 0.15f);
			StationCrystalComponents[StIdx]->SetVariableFloat(FName(TEXT("User.PulseFlash")), 0.0f);
			StationCrystalComponents[StIdx]->SetVariableFloat(FName(TEXT("User.BeatPulse")), 0.0f);
		}
		if (StationCrystalDynamicMaterials.IsValidIndex(StIdx) && StationCrystalDynamicMaterials[StIdx])
		{
			StationCrystalDynamicMaterials[StIdx]->SetScalarParameterValue(TEXT("Brightness"), 0.15f);
			StationCrystalDynamicMaterials[StIdx]->SetScalarParameterValue(TEXT("PulseFlash"), 0.0f);
		}
	}
}

void AHarmonicConvergenceAudioManager::UpdateAttractMode(float DeltaTime)
{
	if (!bEnableAttractMode)
	{
		return;
	}

	// Check if any crystals are currently active and looping
	bool bAnyActiveCrystals = false;
	for (const FPersistentBeatSlotInfo& Slot : PersistentBeatSlots)
	{
		if (Slot.bIsActive)
		{
			bAnyActiveCrystals = true;
			break;
		}
	}

	// Attract mode should ONLY run when installation is completely idle:
	// No buttons are pressed AND no crystals are active
	if (ActiveVoiceCount == 0 && !bAnyActiveCrystals)
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

					// Attract ping briefly glints the station's crystal in the core
					StationPulseFlash[RandomStationIndex] = 0.8f;
					if (StationCrystalComponents.IsValidIndex(RandomStationIndex) && StationCrystalComponents[RandomStationIndex])
					{
						StationCrystalComponents[RandomStationIndex]->SetVariableFloat(FName(TEXT("User.PulseFlash")), 0.8f);
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
		// Installation is actively engaged: reset inactivity timer
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
