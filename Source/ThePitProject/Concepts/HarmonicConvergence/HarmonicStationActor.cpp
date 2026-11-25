#include "HarmonicStationActor.h"
#include "InputManagerSubSystem.h"
#include "Components/SceneComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

AHarmonicStationActor::AHarmonicStationActor()
{
	PrimaryActorTick.bCanEverTick = true;

	StationRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StationRoot"));
#if WITH_EDITORONLY_DATA
	StationRoot->bVisualizeComponent = true;
#endif
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
	if (ChannelName.IsNone() || bAutoAssignAudioProfile)
	{
		ApplyDefaultPresetForIndex();
	}
}

void AHarmonicStationActor::ApplyDefaultPresetForIndex()
{
	// If ChannelName is already set to "player1".."player24", derive StationIndex (0..23) from it
	// so 4-button unit grouping (StationIndex / 4) and synth slots never collide.
	if (!ChannelName.IsNone())
	{
		FString ChannelStr = ChannelName.ToString();
		if (ChannelStr.StartsWith(TEXT("player"), ESearchCase::IgnoreCase))
		{
			ChannelStr.RightChopInline(6);
			if (ChannelStr.IsNumeric())
			{
				const int32 PlayerNum = FCString::Atoi(*ChannelStr);
				if (PlayerNum >= 1 && PlayerNum <= 24)
				{
					StationIndex = PlayerNum - 1;
				}
			}
		}
	}

	struct FStationPreset
	{
		const TCHAR* Name;
		EVoiceTimbreProfile Timbre;
		FLinearColor Color;
		int32 PatternMask;
		EChordVoiceRole VoiceRole;
		int32 OctaveOffset;
	};

	static const FStationPreset Presets[24] = {
		// Unit 1 (0–3): South Station - Beats / Sub-Bass
		{ TEXT("player1"),  EVoiceTimbreProfile::SubBassPad,       FLinearColor(0.0f, 0.9f, 1.0f),  0x1111, EChordVoiceRole::LowestVoice,  0 },
		{ TEXT("player2"),  EVoiceTimbreProfile::SubBassPad,       FLinearColor(0.0f, 0.6f, 1.0f),  0x0449, EChordVoiceRole::LowestVoice,  1 },
		{ TEXT("player3"),  EVoiceTimbreProfile::SubBassPad,       FLinearColor(0.1f, 0.4f, 1.0f),  0x1010, EChordVoiceRole::MiddleVoice,  2 },
		{ TEXT("player4"),  EVoiceTimbreProfile::SubBassPad,       FLinearColor(0.0f, 0.8f, 0.9f),  0x4444, EChordVoiceRole::HighestVoice, 3 },

		// Unit 2 (4–7): South-East Station - Bass & Percussive Groove
		{ TEXT("player5"),  EVoiceTimbreProfile::SubBassPad,       FLinearColor(0.2f, 0.5f, 1.0f),  0x0481, EChordVoiceRole::LowestVoice,  1 },
		{ TEXT("player6"),  EVoiceTimbreProfile::SubBassPad,       FLinearColor(0.0f, 1.0f, 0.8f),  0x2041, EChordVoiceRole::MiddleVoice,  2 },
		{ TEXT("player7"),  EVoiceTimbreProfile::SubBassPad,       FLinearColor(0.3f, 0.2f, 0.9f),  0x0905, EChordVoiceRole::LowestVoice,  3 },
		{ TEXT("player8"),  EVoiceTimbreProfile::CrystallineChime, FLinearColor(1.0f, 0.6f, 0.0f),  0x1010, EChordVoiceRole::HighestVoice, 2 },

		// Unit 3 (8–11): North-East Station - Acoustic Percussion & Chimes
		{ TEXT("player9"),  EVoiceTimbreProfile::CrystallineChime, FLinearColor(0.9f, 0.8f, 0.1f),  0x5555, EChordVoiceRole::HighestVoice, 3 },
		{ TEXT("player10"), EVoiceTimbreProfile::CrystallineChime, FLinearColor(1.0f, 0.45f, 0.1f), 0xAAAA, EChordVoiceRole::MiddleVoice,  1 },
		{ TEXT("player11"), EVoiceTimbreProfile::WarmPad,          FLinearColor(0.9f, 0.1f, 0.5f),  0x4444, EChordVoiceRole::LowestVoice,  2 },
		{ TEXT("player12"), EVoiceTimbreProfile::WarmPad,          FLinearColor(1.0f, 0.2f, 0.3f),  0x0001, EChordVoiceRole::HighestVoice, 4 },

		// Unit 4 (12–15): North Station - Harmonic Warm Pads
		{ TEXT("player13"), EVoiceTimbreProfile::WarmPad,          FLinearColor(0.8f, 0.1f, 0.8f),  0x4108, EChordVoiceRole::LowestVoice,  2 },
		{ TEXT("player14"), EVoiceTimbreProfile::WarmPad,          FLinearColor(0.6f, 0.1f, 0.9f),  0x0410, EChordVoiceRole::MiddleVoice,  2 },
		{ TEXT("player15"), EVoiceTimbreProfile::WarmPad,          FLinearColor(1.0f, 0.3f, 0.6f),  0x1084, EChordVoiceRole::HighestVoice, 4 },
		{ TEXT("player16"), EVoiceTimbreProfile::WarmPad,          FLinearColor(0.7f, 0.2f, 0.7f),  0x4040, EChordVoiceRole::LowestVoice,  1 },

		// Unit 5 (16–19): North-West Station - Melodic Leads & Vortex Sweeps
		{ TEXT("player17"), EVoiceTimbreProfile::WarmPad,          FLinearColor(0.9f, 0.4f, 0.2f),  0x0101, EChordVoiceRole::MiddleVoice,  3 },
		{ TEXT("player18"), EVoiceTimbreProfile::VortexSweep,      FLinearColor(0.1f, 1.0f, 0.4f),  0x0A28, EChordVoiceRole::HighestVoice, 4 },
		{ TEXT("player19"), EVoiceTimbreProfile::CrystallineChime, FLinearColor(0.3f, 1.0f, 0.7f),  0x2492, EChordVoiceRole::LowestVoice,  2 },
		{ TEXT("player20"), EVoiceTimbreProfile::VortexSweep,      FLinearColor(1.0f, 1.0f, 1.0f),  0xD000, EChordVoiceRole::MiddleVoice,  3 },

		// Unit 6 (20–23): South-West Station - Crystalline Resonances & Shimmers
		{ TEXT("player21"), EVoiceTimbreProfile::CrystallineChime, FLinearColor(0.2f, 0.8f, 1.0f),  0x5050, EChordVoiceRole::MiddleVoice,  2 },
		{ TEXT("player22"), EVoiceTimbreProfile::WarmPad,          FLinearColor(0.9f, 0.5f, 1.0f),  0x0220, EChordVoiceRole::HighestVoice, 3 },
		{ TEXT("player23"), EVoiceTimbreProfile::VortexSweep,      FLinearColor(1.0f, 0.8f, 0.3f),  0x1441, EChordVoiceRole::LowestVoice,  4 },
		{ TEXT("player24"), EVoiceTimbreProfile::SubBassPad,       FLinearColor(0.3f, 1.0f, 0.5f),  0x8888, EChordVoiceRole::LowestVoice,  1 }
	};

	const int32 ClampedIdx = FMath::Clamp(StationIndex, 0, 23);
	if (ChannelName.IsNone())
	{
		ChannelName = FName(Presets[ClampedIdx].Name);
	}
	TimbreProfile = Presets[ClampedIdx].Timbre;
	RhythmicPatternMask = Presets[ClampedIdx].PatternMask;
	ChordVoiceRole = Presets[ClampedIdx].VoiceRole;
	OctaveOffset = Presets[ClampedIdx].OctaveOffset;
	// Spread the 6 perimeter units around the stereo field (-0.85 .. +0.85)
	const int32 UnitIdx = ClampedIdx / 4;
	PanPosition = -0.85f + (static_cast<float>(UnitIdx) / 5.0f) * 1.70f;
}

void AHarmonicStationActor::BeginPlay()
{
	Super::BeginPlay();

	if (ChannelName.IsNone() || bAutoAssignAudioProfile)
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
			Comp->SetVariableFloat(FName(TEXT("User.myVortexForce")), VortexForce);
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

static void GetTimbreVisualMultipliers(EVoiceTimbreProfile Profile, float& OutSpawnMult, float& OutVortexMult)
{
	switch (Profile)
	{
	case EVoiceTimbreProfile::SubBassPad:       OutSpawnMult = 1.8f; OutVortexMult = 0.5f;  break;
	case EVoiceTimbreProfile::CrystallineChime: OutSpawnMult = 1.2f; OutVortexMult = 0.9f;  break;
	case EVoiceTimbreProfile::WarmPad:          OutSpawnMult = 1.5f; OutVortexMult = 0.75f; break;
	case EVoiceTimbreProfile::VortexSweep:      OutSpawnMult = 1.4f; OutVortexMult = 1.35f; break;
	default:                                    OutSpawnMult = 1.0f; OutVortexMult = 1.0f;  break;
	}
}

void AHarmonicStationActor::SetPressed(bool bNewPressed)
{
	if (bIsPressed == bNewPressed)
	{
		return;
	}

	bIsPressed = bNewPressed;

	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	if (bIsPressed)
	{
		LastPressTimestamp = CurrentTime;
		if (CrystalActor)
		{
			if (CrystalActor->bIsActive)
			{
				// PULSE-TO-SUSTAIN: Pressing an active station feeds it extra lifespan + triggers a big pulse
				CrystalActor->RemainingLifespan = FMath::Min(
					CrystalActor->ActiveLifespanSeconds,
					CrystalActor->RemainingLifespan + CrystalActor->ActiveLifespanSeconds * 0.5f);
				CrystalActor->TriggerBeatPulse(1.5f);
			}
			CrystalActor->SetCharging(true);
		}
	}
	else if (CrystalActor)
	{
		CrystalActor->SetCharging(false);
	}

	if (BeamComponent)
	{
		float SpawnMult = 1.0f, VortexMult = 1.0f;
		GetTimbreVisualMultipliers(TimbreProfile, SpawnMult, VortexMult);
		if (bIsPressed)
		{
			const float StartRate = IsCrystalActive() ? MaxSpawnRate : SpawnRate;
			BeamComponent->SetVariableVec3(FName(TEXT("User.BeamStartLocation")), GetActorLocation() - GetCrystalWorldLocation());
			BeamComponent->SetVariableFloat(FName(TEXT("User.mySpawnRate")), StartRate * SpawnMult);
			BeamComponent->SetVariableFloat(FName(TEXT("User.myVortexForce")), VortexForce * VortexMult);
		}
		else
		{
			const bool bCrystalActive = IsCrystalActive();
			BeamComponent->SetVariableFloat(FName(TEXT("User.mySpawnRate")), bCrystalActive ? SpawnRate * SpawnMult * TrickleSpawnMultiplier : 0.0f);
			BeamComponent->SetVariableFloat(FName(TEXT("User.myVortexForce")), bCrystalActive ? TrickleVortexForce * VortexMult : 0.0f);
		}
	}

	OnStationPressedChanged.Broadcast(this, bIsPressed);
}

void AHarmonicStationActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!BeamComponent)
	{
		return;
	}

	float SpawnMult = 1.0f, VortexMult = 1.0f;
	GetTimbreVisualMultipliers(TimbreProfile, SpawnMult, VortexMult);

	if (bIsPressed)
	{
		const float ChargeAlpha = IsCrystalActive() ? 1.0f : FMath::Clamp(GetChargeProgress(), 0.0f, 1.0f);
		const float CurrentSpawnRate = FMath::Lerp(SpawnRate, MaxSpawnRate, ChargeAlpha);
		BeamComponent->SetVariableFloat(FName(TEXT("User.mySpawnRate")), CurrentSpawnRate * SpawnMult);
		BeamComponent->SetVariableFloat(FName(TEXT("User.myVortexForce")), VortexForce * VortexMult);
	}
	else if (IsCrystalActive())
	{
		float WarningPulse = 1.0f;
		if (CrystalActor && CrystalActor->RemainingLifespan <= 5.0f)
		{
			const float Wave = 0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * 10.0f);
			WarningPulse = FMath::Lerp(0.35f, 1.15f, Wave);
		}
		BeamComponent->SetVariableFloat(FName(TEXT("User.mySpawnRate")), SpawnRate * SpawnMult * TrickleSpawnMultiplier * WarningPulse);
		BeamComponent->SetVariableFloat(FName(TEXT("User.myVortexForce")), TrickleVortexForce * VortexMult * WarningPulse);
	}
}

void AHarmonicStationActor::UpdateBeam()
{
	if (BeamComponent && !bIsPressed)
	{
		float SpawnMult = 1.0f, VortexMult = 1.0f;
		GetTimbreVisualMultipliers(TimbreProfile, SpawnMult, VortexMult);
		const bool bCrystalActive = IsCrystalActive();
		BeamComponent->SetVariableFloat(FName(TEXT("User.mySpawnRate")), bCrystalActive ? SpawnRate * SpawnMult * TrickleSpawnMultiplier : 0.0f);
		BeamComponent->SetVariableFloat(FName(TEXT("User.myVortexForce")), bCrystalActive ? TrickleVortexForce * VortexMult : 0.0f);
	}
}

FVector AHarmonicStationActor::GetCrystalWorldLocation() const
{
	return CrystalActor ? CrystalActor->GetActorLocation() : GetActorLocation();
}
