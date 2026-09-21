// Fill out your copyright notice in the Description page of Project Settings.


#include "GM_ThePitManager.h"
#include "InputManagerSubSystem.h"

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


void AGM_ThePitManager::HandleButtonPressed(FName Channel)
{
	if (CurrentGamePhase == EPitGamePhases::Registration)
	{
		if (!ActiveButtonChannels.Contains(Channel))
		{
			int32 NewPlayerIndex = ActiveButtonChannels.Num();
			ActiveButtonChannels.Add(Channel);
			ChannelToPlayerIndex.Add(Channel, NewPlayerIndex);
			OnPlayerRegistered(NewPlayerIndex, Channel);
			UE_LOG(LogTemp, Log, TEXT("Player %d joined with button: %s"), NewPlayerIndex, *Channel.ToString());
		}
	}
	
	if (CurrentGamePhase == EPitGamePhases::Gameplay)
	{
		if (const int32* PlayerIndex = ChannelToPlayerIndex.Find(Channel))
		{
			OnPlayerAction(*PlayerIndex, Channel);
			
			if (AActor* Actor = GetPlayerActor(*PlayerIndex))
			{
				if (Actor->Implements<UPitControllableInterface>())
				{
					IPitControllableInterface::Execute_OnActionPressed(Actor, Channel);
				}
			}
			
			UE_LOG(LogTemp, Log, TEXT("Player %d pressed action button"), *PlayerIndex);
		}
	}
}

void AGM_ThePitManager::HandleButtonReleased(FName Channel)
{
	if (CurrentGamePhase == EPitGamePhases::Gameplay)
	{
		if (const int32* PlayerIndex = ChannelToPlayerIndex.Find(Channel))
		{
			OnPlayerButtonReleased(*PlayerIndex, Channel);
			
			if (AActor* Actor = GetPlayerActor(*PlayerIndex))
			{
				if (Actor->Implements<UPitControllableInterface>())
				{
					IPitControllableInterface::Execute_OnActionReleased(Actor, Channel);
				}
			}
		}
	}
}

void AGM_ThePitManager::HandleInputChanged(FName Channel, float Value, float Delta)
{
	if (CurrentGamePhase == EPitGamePhases::Gameplay)
	{
		if (const int32* PlayerIndex = ChannelToPlayerIndex.Find(Channel))
		{
			OnPlayerInputChanged(*PlayerIndex, Channel, Value, Delta);
			
			if (AActor* Actor = GetPlayerActor(*PlayerIndex))
			{
				if (Actor->Implements<UPitControllableInterface>())
				{
					IPitControllableInterface::Execute_OnAxisInput(Actor, Channel, Value, Delta);
				}
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
