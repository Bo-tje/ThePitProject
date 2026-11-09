#include "HarmonicStationActor.h"
#include "InputManagerSubSystem.h"
#include "Components/SceneComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Quartz/AudioMixerClockHandle.h"
#include "UObject/ConstructorHelpers.h"

AHarmonicStationActor::AHarmonicStationActor()
{
	PrimaryActorTick.bCanEverTick = false;

	StationRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StationRoot"));
	SetRootComponent(StationRoot);

	static ConstructorHelpers::FObjectFinderOptional<UNiagaraSystem> DefaultBeamSystem(TEXT("/Game/Niagara/NS_Empty.NS_Empty"));
	if (DefaultBeamSystem.Succeeded())
	{
		StationBeamSystem = DefaultBeamSystem.Get();
	}
}

void AHarmonicStationActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (ChannelName.IsNone())
	{
		ApplyDefaultPresetForIndex();
	}
}

void AHarmonicStationActor::ApplyDefaultPresetForIndex()
{
	struct FStationPreset
	{
		const TCHAR* Name;
		EVoiceTimbreProfile Timbre;
		FLinearColor Color;
		int32 PatternMask;
	};

	static const FStationPreset Presets[20] = {
		// South Railing (0–6): Beats / Sub-Bass
		{ TEXT("player1"),  EVoiceTimbreProfile::SubBassPad,       FLinearColor(0.0f, 0.9f, 1.0f),  0x1111 },
		{ TEXT("player2"),  EVoiceTimbreProfile::SubBassPad,       FLinearColor(0.0f, 0.6f, 1.0f),  0x0449 },
		{ TEXT("player3"),  EVoiceTimbreProfile::SubBassPad,       FLinearColor(0.1f, 0.4f, 1.0f),  0x0101 },
		{ TEXT("player4"),  EVoiceTimbreProfile::SubBassPad,       FLinearColor(0.0f, 0.8f, 0.9f),  0x4444 },
		{ TEXT("player5"),  EVoiceTimbreProfile::SubBassPad,       FLinearColor(0.2f, 0.5f, 1.0f),  0x0481 },
		{ TEXT("player6"),  EVoiceTimbreProfile::SubBassPad,       FLinearColor(0.0f, 1.0f, 0.8f),  0x2041 },
		{ TEXT("player7"),  EVoiceTimbreProfile::SubBassPad,       FLinearColor(0.3f, 0.2f, 0.9f),  0x0905 },

		// East Railing (7–9): Acoustic Percussion
		{ TEXT("player8"),  EVoiceTimbreProfile::CrystallineChime, FLinearColor(1.0f, 0.6f, 0.0f),  0x1010 },
		{ TEXT("player9"),  EVoiceTimbreProfile::CrystallineChime, FLinearColor(0.9f, 0.8f, 0.1f),  0x5555 },
		{ TEXT("player10"), EVoiceTimbreProfile::CrystallineChime, FLinearColor(1.0f, 0.45f, 0.1f), 0xAAAA },

		// North Railing (10–16): Harmonic Warm Pads
		{ TEXT("player11"), EVoiceTimbreProfile::WarmPad,          FLinearColor(0.9f, 0.1f, 0.5f),  0x4444 },
		{ TEXT("player12"), EVoiceTimbreProfile::WarmPad,          FLinearColor(1.0f, 0.2f, 0.3f),  0x0001 },
		{ TEXT("player13"), EVoiceTimbreProfile::WarmPad,          FLinearColor(0.8f, 0.1f, 0.8f),  0x4108 },
		{ TEXT("player14"), EVoiceTimbreProfile::WarmPad,          FLinearColor(0.6f, 0.1f, 0.9f),  0x0410 },
		{ TEXT("player15"), EVoiceTimbreProfile::WarmPad,          FLinearColor(1.0f, 0.3f, 0.6f),  0x1084 },
		{ TEXT("player16"), EVoiceTimbreProfile::WarmPad,          FLinearColor(0.7f, 0.2f, 0.7f),  0x4040 },
		{ TEXT("player17"), EVoiceTimbreProfile::WarmPad,          FLinearColor(0.9f, 0.4f, 0.2f),  0x0101 },

		// West Railing (17–19): Melodic Leads & Chimes
		{ TEXT("player18"), EVoiceTimbreProfile::VortexSweep,      FLinearColor(0.1f, 1.0f, 0.4f),  0x0A28 },
		{ TEXT("player19"), EVoiceTimbreProfile::CrystallineChime, FLinearColor(0.3f, 1.0f, 0.7f),  0x2492 },
		{ TEXT("player20"), EVoiceTimbreProfile::VortexSweep,      FLinearColor(1.0f, 1.0f, 1.0f),  0xD000 }
	};

	const int32 ClampedIdx = FMath::Clamp(StationIndex, 0, 19);
	ChannelName = FName(Presets[ClampedIdx].Name);
	TimbreProfile = Presets[ClampedIdx].Timbre;
	StreamColor = Presets[ClampedIdx].Color;
	RhythmicPatternMask = Presets[ClampedIdx].PatternMask;
}

void AHarmonicStationActor::BeginPlay()
{
	Super::BeginPlay();

	if (ChannelName.IsNone())
	{
		ApplyDefaultPresetForIndex();
	}

	// 1. Spawn Beam Niagara VFX aimed at the linked CrystalActor
	if (StationBeamSystem)
	{
		const FVector CrystalLoc = GetCrystalWorldLocation();
		if (UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), StationBeamSystem, CrystalLoc, FRotator::ZeroRotator, FVector(1.0f), false, true))
		{
			const FVector DynamicColorVec = FVector(StreamColor.R, StreamColor.G, StreamColor.B) * 30.0f;
			Comp->SetVariableVec3(FName(TEXT("User.CenterLocation")), FVector::ZeroVector);
			Comp->SetVariableVec3(FName(TEXT("User.myOrigingForce")), FVector::ZeroVector);
			Comp->SetVariableVec3(FName(TEXT("User.BeamStartLocation")), GetActorLocation() - CrystalLoc);
			Comp->SetVariableVec3(FName(TEXT("User.BeamColor")), DynamicColorVec);
			Comp->SetVariableVec3(FName(TEXT("User.myColor")), DynamicColorVec);
			Comp->SetVariableLinearColor(FName(TEXT("User.BeamColor")), StreamColor);
			Comp->SetVariableLinearColor(FName(TEXT("User.myColor")), StreamColor);
			Comp->SetVariableFloat(FName(TEXT("User.myVortexForce")), SpawnRate * 1.5f);
			Comp->SetVariableFloat(FName(TEXT("User.mySpawnRate")), 0.0f);
			BeamComponent = Comp;
		}
	}

	// 2. Initialize linked CrystalActor
	if (CrystalActor)
	{
		CrystalActor->SetupCrystal(StationIndex, StreamColor);
	}

	// 3. Subscribe directly to OSC input events for this station's ChannelName
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UInputManagerSubSystem* InputSubsystem = GI->GetSubsystem<UInputManagerSubSystem>())
		{
			InputSubsystem->OnButtonPressed.AddDynamic(this, &AHarmonicStationActor::HandleOSCButtonPressed);
			InputSubsystem->OnButtonReleased.AddDynamic(this, &AHarmonicStationActor::HandleOSCButtonReleased);
			InputSubsystem->OnInputChanged.AddDynamic(this, &AHarmonicStationActor::HandleOSCInputChanged);
		}
	}
}

void AHarmonicStationActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UInputManagerSubSystem* InputSubsystem = GI->GetSubsystem<UInputManagerSubSystem>())
		{
			InputSubsystem->OnButtonPressed.RemoveDynamic(this, &AHarmonicStationActor::HandleOSCButtonPressed);
			InputSubsystem->OnButtonReleased.RemoveDynamic(this, &AHarmonicStationActor::HandleOSCButtonReleased);
			InputSubsystem->OnInputChanged.RemoveDynamic(this, &AHarmonicStationActor::HandleOSCInputChanged);
		}
	}
	if (BeamComponent)
	{
		BeamComponent->DeactivateImmediate();
	}

	Super::EndPlay(EndPlayReason);
}

void AHarmonicStationActor::HandleOSCButtonPressed(FName InChannel)
{
	if (InChannel == ChannelName)
	{
		SetPressed(true);
	}
}

void AHarmonicStationActor::HandleOSCButtonReleased(FName InChannel)
{
	if (InChannel == ChannelName)
	{
		SetPressed(false);
	}
}

void AHarmonicStationActor::HandleOSCInputChanged(FName InChannel, float InputValue, float ValueDelta)
{
	if (InChannel == ChannelName)
	{
		CurrentPressure = FMath::Clamp(InputValue, 0.0f, 1.0f);
	}
}

void AHarmonicStationActor::SetPressed(bool bNewPressed)
{
	if (bIsPressed == bNewPressed)
	{
		return;
	}

	bIsPressed = bNewPressed;
	CurrentPressure = bIsPressed ? 1.0f : 0.0f;

	if (CrystalActor)
	{
		CrystalActor->SetCharging(bIsPressed);
	}

	if (BeamComponent)
	{
		if (bIsPressed)
		{
			BeamComponent->SetVariableVec3(FName(TEXT("User.BeamStartLocation")), GetActorLocation() - GetCrystalWorldLocation());
			BeamComponent->SetVariableFloat(FName(TEXT("User.mySpawnRate")), SpawnRate);
			BeamComponent->SetVariableFloat(FName(TEXT("User.myVortexForce")), SpawnRate * 1.5f);
		}
		else
		{
			BeamComponent->SetVariableFloat(FName(TEXT("User.mySpawnRate")), 0.0f);
		}
	}

	OnStationPressedChanged.Broadcast(this, bIsPressed);
}

void AHarmonicStationActor::InitializeAudioStem(UQuartzClockHandle* MasterClock)
{
	if (!AudioStemAsset)
	{
		return;
	}

	AudioStemAsset->bLooping = true;
	StemAudioComponent = NewObject<UAudioComponent>(this);
	StemAudioComponent->SetSound(AudioStemAsset);
	StemAudioComponent->bAutoActivate = false;
	StemAudioComponent->bAutoDestroy = false;
	StemAudioComponent->bIsUISound = false;
	StemAudioComponent->SetVolumeMultiplier(0.0f);
	StemAudioComponent->AttachToComponent(StationRoot, FAttachmentTransformRules::KeepRelativeTransform);
	StemAudioComponent->RegisterComponent();

	if (MasterClock)
	{
		FQuartzQuantizationBoundary BarBoundary;
		BarBoundary.Quantization = EQuartzCommandQuantization::Bar;
		FOnQuartzCommandEventBP DummyDelegate;
		StemAudioComponent->PlayQuantized(this, MasterClock, BarBoundary, DummyDelegate, 0.0f, 0.0f, 1.0f);
	}
	else
	{
		StemAudioComponent->Play();
	}
}

void AHarmonicStationActor::UpdateBeamAndStem(float DeltaTime, float ConvergenceEnergy, int32 ActiveStationCount, float CoreBrightness)
{
	const float Charge = GetChargeProgress();
	if (BeamComponent)
	{
		BeamComponent->SetVariableFloat(FName(TEXT("User.ConvergenceEnergy")), ConvergenceEnergy);
		BeamComponent->SetVariableInt(FName(TEXT("User.ActiveStationCount")), ActiveStationCount);
		BeamComponent->SetVariableFloat(FName(TEXT("User.CoreBrightness")), CoreBrightness);
		BeamComponent->SetVariableFloat(FName(TEXT("User.VoiceModulation")), bIsPressed ? Charge : 0.0f);
	}

	if (StemAudioComponent)
	{
		if (IsCrystalActive())
		{
			if (!StemAudioComponent->IsPlaying())
			{
				StemAudioComponent->Play();
			}
			const float Remaining = CrystalActor->RemainingLifespan;
			const float TargetAlpha = (Remaining < 6.0f) ? FMath::Clamp(Remaining / 6.0f, 0.0f, 1.0f) : 1.0f;
			StemAudioComponent->SetVolumeMultiplier(FMath::FInterpTo(StemAudioComponent->VolumeMultiplier, TargetAlpha, DeltaTime, 8.0f));
		}
		else if (StemAudioComponent->VolumeMultiplier > 0.001f)
		{
			StemAudioComponent->SetVolumeMultiplier(FMath::FInterpTo(StemAudioComponent->VolumeMultiplier, 0.0f, DeltaTime, 6.0f));
		}
	}
}

FVector AHarmonicStationActor::GetCrystalWorldLocation() const
{
	return CrystalActor ? CrystalActor->GetActorLocation() : GetActorLocation();
}

void AHarmonicStationActor::ResetStation()
{
	if (StemAudioComponent)
	{
		StemAudioComponent->SetVolumeMultiplier(0.0f);
	}
	if (CrystalActor)
	{
		CrystalActor->ResetCrystal();
	}
}
