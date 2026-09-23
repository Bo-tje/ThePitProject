// Fill out your copyright notice in the Description page of Project Settings.

#include "GM_ThePitManager.h"
#include "InputManagerSubSystem.h"
#include "PitShootyPlayer.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

void AGM_ThePitManager::BeginPlay()
{
	Super::BeginPlay();
	
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UInputManagerSubSystem* InputSubsystem = GameInstance->GetSubsystem<UInputManagerSubSystem>())
		{
			InputSubsystem->OnButtonPressed.AddDynamic(this, &AGM_ThePitManager::HandleButtonPressed);
			InputSubsystem->OnButtonReleased.AddDynamic(this, &AGM_ThePitManager::HandleButtonReleased);
			InputSubsystem->OnInputChanged.AddDynamic(this, &AGM_ThePitManager::HandleInputChanged);
		}
	}
}

void AGM_ThePitManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UInputManagerSubSystem* InputSubsystem = GameInstance->GetSubsystem<UInputManagerSubSystem>())
		{
			InputSubsystem->OnButtonPressed.RemoveDynamic(this, &AGM_ThePitManager::HandleButtonPressed);
			InputSubsystem->OnButtonReleased.RemoveDynamic(this, &AGM_ThePitManager::HandleButtonReleased);
			InputSubsystem->OnInputChanged.RemoveDynamic(this, &AGM_ThePitManager::HandleInputChanged);
		}
	}
	
	Super::EndPlay(EndPlayReason);
}

bool AGM_ThePitManager::ResolvePlayerIndexAndSubChannel(FName InFullChannel, int32& OutPlayerIndex, FName& OutSubChannel) const
{
	FString FullStr = InFullChannel.ToString();
	FString PlayerPrefix;
	FString SubStr;

	// Split by dot (e.g. "player0.action" -> "player0", "action")
	if (FullStr.Split(TEXT("."), &PlayerPrefix, &SubStr))
	{
		OutSubChannel = FName(*SubStr);
	}
	else
	{
		PlayerPrefix = FullStr;
		OutSubChannel = NAME_None;
	}

	// Try extracting player number from prefix: "player0", "p1", "0", etc.
	for (int32 i = 0; i < PlayerPrefix.Len(); ++i)
	{
		if (FChar::IsDigit(PlayerPrefix[i]))
		{
			OutPlayerIndex = FCString::Atoi(*PlayerPrefix + i);
			return true;
		}
	}

	// Fallback to registered channel map
	if (const int32* FoundIndex = ChannelToPlayerIndex.Find(InFullChannel))
	{
		OutPlayerIndex = *FoundIndex;
		return true;
	}
	if (const int32* FoundIndex = ChannelToPlayerIndex.Find(FName(*PlayerPrefix)))
	{
		OutPlayerIndex = *FoundIndex;
		return true;
	}

	OutPlayerIndex = -1;
	return false;
}

void AGM_ThePitManager::AssignActorToPlayer(int32 PlayerIndex, AActor* TargetActor)
{
	PlayerActors.Add(PlayerIndex, TargetActor);
    
	if (TargetActor && TargetActor->Implements<UPitControllableInterface>())
	{
		FName Channel = ActiveButtonChannels.IsValidIndex(PlayerIndex) ? ActiveButtonChannels[PlayerIndex] : NAME_None;
		IPitControllableInterface::Execute_OnPlayerAssigned(TargetActor, PlayerIndex, Channel);
	}
}
    
AActor* AGM_ThePitManager::GetPlayerActor(int32 PlayerIndex) const
{
	if (const TObjectPtr<AActor>* FoundActor = PlayerActors.Find(PlayerIndex))
	{
		return *FoundActor;
	}
	return nullptr;
}

AActor* AGM_ThePitManager::SpawnPlayerForIndex(int32 PlayerIndex)
{
	UWorld* World = GetWorld();
	if (!World || !PlayerPawnClass)
	{
		return nullptr;
	}

	// 1. Find matching Track Actor (by tag "Player0", "Player1", etc. or by index)
	AActor* AssignedTrackActor = nullptr;
	FName PlayerTagName = *FString::Printf(TEXT("Player%d"), PlayerIndex);

	if (TrackActorClass)
	{
		TArray<AActor*> FoundTracks;
		UGameplayStatics::GetAllActorsOfClass(World, TrackActorClass, FoundTracks);

		for (AActor* Track : FoundTracks)
		{
			if (Track && Track->ActorHasTag(PlayerTagName))
			{
				AssignedTrackActor = Track;
				break;
			}
		}

		if (!AssignedTrackActor && FoundTracks.IsValidIndex(PlayerIndex))
		{
			AssignedTrackActor = FoundTracks[PlayerIndex];
		}
	}

	// 2. Determine initial spawn transform
	FTransform SpawnTransform = FTransform::Identity;
	if (PredefinedSpawnTransforms.IsValidIndex(PlayerIndex))
	{
		SpawnTransform = PredefinedSpawnTransforms[PlayerIndex];
	}
	else if (AssignedTrackActor)
	{
		SpawnTransform = AssignedTrackActor->GetActorTransform();
	}

	// 3. Spawn the player
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* SpawnedPlayer = World->SpawnActor<AActor>(PlayerPawnClass, SpawnTransform, SpawnParams);

	if (SpawnedPlayer)
	{
		// If it's a PitShootyPlayer, bind track actor
		if (APitShootyPlayer* ShootyPlayer = Cast<APitShootyPlayer>(SpawnedPlayer))
		{
			if (AssignedTrackActor)
			{
				ShootyPlayer->SetTrackActor(AssignedTrackActor);
				ShootyPlayer->UpdateTransformOnSpline();
			}
		}

		AssignActorToPlayer(PlayerIndex, SpawnedPlayer);
		UE_LOG(LogTemp, Log, TEXT("Auto-spawned Player %d (%s)"), PlayerIndex, *SpawnedPlayer->GetName());
	}

	return SpawnedPlayer;
}

void AGM_ThePitManager::HandleButtonPressed(FName Channel)
{
	int32 PlayerIndex = -1;
	FName SubChannel = NAME_None;
	bool bFound = ResolvePlayerIndexAndSubChannel(Channel, PlayerIndex, SubChannel);

	FString SubStr = SubChannel.ToString().ToLower();
	
	if (CurrentGamePhase == EPitGamePhases::Registration)
	{
		if (!bFound || PlayerIndex < 0)
		{
			PlayerIndex = ActiveButtonChannels.Num();
			ChannelToPlayerIndex.Add(Channel, PlayerIndex);
		}

		if (!ActiveButtonChannels.Contains(Channel))
		{
			ActiveButtonChannels.Add(Channel);
			ChannelToPlayerIndex.FindOrAdd(Channel, PlayerIndex);

			if (bAutoSpawnOnRegister && PlayerPawnClass && !PlayerActors.Contains(PlayerIndex))
			{
				SpawnPlayerForIndex(PlayerIndex);
			}

			OnPlayerRegistered(PlayerIndex, Channel);
			UE_LOG(LogTemp, Log, TEXT("Player %d joined with channel: %s"), PlayerIndex, *Channel.ToString());
		}
	}
	
	if (CurrentGamePhase == EPitGamePhases::Gameplay && bFound && PlayerIndex >= 0)
	{
		const FName TargetSub = SubChannel.IsNone() ? Channel : SubChannel;
		OnPlayerAction(PlayerIndex, TargetSub);
		if (AActor* Actor = GetPlayerActor(PlayerIndex))
		{
			if (Actor->Implements<UPitControllableInterface>())
			{
				IPitControllableInterface::Execute_OnActionPressed(Actor, TargetSub);
			}
		}
		UE_LOG(LogTemp, Log, TEXT("Player %d pressed action (subchannel: %s)"), PlayerIndex, *SubChannel.ToString());
	}
}

void AGM_ThePitManager::HandleButtonReleased(FName Channel)
{
	int32 PlayerIndex = -1;
	FName SubChannel = NAME_None;
	if (CurrentGamePhase == EPitGamePhases::Gameplay && ResolvePlayerIndexAndSubChannel(Channel, PlayerIndex, SubChannel) && PlayerIndex >= 0)
	{
		const FName TargetSub = SubChannel.IsNone() ? Channel : SubChannel;
		OnPlayerButtonReleased(PlayerIndex, TargetSub);
		if (AActor* Actor = GetPlayerActor(PlayerIndex))
		{
			if (Actor->Implements<UPitControllableInterface>())
			{
				IPitControllableInterface::Execute_OnActionReleased(Actor, TargetSub);
			}
		}
		UE_LOG(LogTemp, Log, TEXT("Player %d released action (subchannel: %s)"), PlayerIndex, *SubChannel.ToString());
	}
}

void AGM_ThePitManager::HandleInputChanged(FName Channel, float Value, float Delta)
{
	int32 PlayerIndex = -1;
	FName SubChannel = NAME_None;
	if (CurrentGamePhase == EPitGamePhases::Gameplay && ResolvePlayerIndexAndSubChannel(Channel, PlayerIndex, SubChannel) && PlayerIndex >= 0)
	{
		const FName TargetSub = SubChannel.IsNone() ? Channel : SubChannel;
		OnPlayerInputChanged(PlayerIndex, TargetSub, Value, Delta);
		if (AActor* Actor = GetPlayerActor(PlayerIndex))
		{
			if (Actor->Implements<UPitControllableInterface>())
			{
				IPitControllableInterface::Execute_OnAxisInput(Actor, TargetSub, Value, Delta);
			}
		}
	}
}

void AGM_ThePitManager::StartGame()
{
	if (ActiveButtonChannels.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("No active button channels"));
		return;
	}
	
	CurrentGamePhase = EPitGamePhases::Gameplay;
	OnGamePhaseChanged(CurrentGamePhase);
	UE_LOG(LogTemp, Log, TEXT("Game Started with %d players"), ActiveButtonChannels.Num());
}
