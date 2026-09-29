#include "FlockCrowdManager.h"
#include "InputManagerSubSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

AFlockCrowdManager::AFlockCrowdManager()
{
	PrimaryActorTick.bCanEverTick = false;
	StationActiveStates.Init(false, 4);
}

void AFlockCrowdManager::BeginPlay()
{
	Super::BeginPlay();

	// Bind to OSC Input Subsystem
	if (UGameInstance* GI = GetGameInstance())
	{
		InputSubsystem = GI->GetSubsystem<UInputManagerSubSystem>();
		if (InputSubsystem)
		{
			InputSubsystem->OnInputChanged.AddDynamic(this, &AFlockCrowdManager::HandleInputChanged);
		}
	}

	SpawnFlockCrowd();
}

void AFlockCrowdManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (InputSubsystem)
	{
		InputSubsystem->OnInputChanged.RemoveDynamic(this, &AFlockCrowdManager::HandleInputChanged);
	}

	Super::EndPlay(EndPlayReason);
}

void AFlockCrowdManager::SpawnFlockCrowd()
{
	if (!CrowdAgentClass || !GetWorld())
	{
		return;
	}

	ActiveAgents.Empty();

	for (int32 i = 0; i < CrowdSize; ++i)
	{
		const float RandX = FMath::RandRange(-SpawnAreaExtents.X, SpawnAreaExtents.X);
		const float RandY = FMath::RandRange(-SpawnAreaExtents.Y, SpawnAreaExtents.Y);
		const FVector SpawnLoc = PitCenterLocation + FVector(RandX, RandY, 100.0f);
		const FRotator SpawnRot = FRotator(0.0f, FMath::RandRange(0.0f, 360.0f), 0.0f);

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		if (AFlockCrowdAgent* NewAgent = GetWorld()->SpawnActor<AFlockCrowdAgent>(CrowdAgentClass, SpawnLoc, SpawnRot, SpawnParams))
		{
			ActiveAgents.Add(NewAgent);
		}
	}
}

void AFlockCrowdManager::HandleInputChanged(FName Channel, float Value, float Delta)
{
	const FString ChannelStr = Channel.ToString().ToLower();
	const bool bPressed = Value > 0.5f;

	if (ChannelStr == TEXT("player1") || ChannelStr.Contains(TEXT("station1")))
	{
		OnStationInputChanged(0, bPressed);
	}
	else if (ChannelStr == TEXT("player2") || ChannelStr.Contains(TEXT("station2")))
	{
		OnStationInputChanged(1, bPressed);
	}
	else if (ChannelStr == TEXT("player3") || ChannelStr.Contains(TEXT("station3")))
	{
		OnStationInputChanged(2, bPressed);
	}
	else if (ChannelStr == TEXT("player4") || ChannelStr.Contains(TEXT("station4")))
	{
		OnStationInputChanged(3, bPressed);
	}
}

void AFlockCrowdManager::OnStationInputChanged(int32 StationIndex, bool bIsPressed)
{
	if (StationActiveStates.IsValidIndex(StationIndex))
	{
		StationActiveStates[StationIndex] = bIsPressed;
		UpdateFlockBehavior();
	}
}

void AFlockCrowdManager::UpdateFlockBehavior()
{
	TArray<FVector> ActiveStationTargets;

	if (StationActiveStates[0]) ActiveStationTargets.Add(StationWestLocation);
	if (StationActiveStates[1]) ActiveStationTargets.Add(StationNorthLocation);
	if (StationActiveStates[2]) ActiveStationTargets.Add(StationEastLocation);
	if (StationActiveStates[3]) ActiveStationTargets.Add(StationSouthLocation);

	// Case 1: No stations active -> Return to ambient wandering
	if (ActiveStationTargets.Num() == 0)
	{
		for (AFlockCrowdAgent* Agent : ActiveAgents)
		{
			if (Agent)
			{
				Agent->ReturnToIdle();
			}
		}
		return;
	}

	// Case 2: All 4 stations active (or 3+) -> FLASH MOB RAVE IN CENTER!
	if (ActiveStationTargets.Num() >= 3)
	{
		for (AFlockCrowdAgent* Agent : ActiveAgents)
		{
			if (Agent)
			{
				Agent->TriggerFlashMob(PitCenterLocation);
			}
		}
		return;
	}

	// Case 3: 1 or 2 stations active -> Steer or split flock
	for (int32 i = 0; i < ActiveAgents.Num(); ++i)
	{
		AFlockCrowdAgent* Agent = ActiveAgents[i];
		if (!Agent) continue;

		// If 2 stations active (e.g. West vs East Tug of War), split agents across active stations
		const int32 TargetIdx = i % ActiveStationTargets.Num();
		const FVector TargetLoc = ActiveStationTargets[TargetIdx];

		Agent->SetTargetStationLocation(TargetLoc, true);
	}
}
