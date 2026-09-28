// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PitControllableInterface.h"
#include "GM_ThePitManager.generated.h"

UENUM(BlueprintType)
enum class EPitGamePhases : uint8
{
	Registration,
	Gameplay,
	GameOver
};
/**
 * 
 */
UCLASS()
class THEPITPROJECT_API AGM_ThePitManager : public AGameModeBase
{
	GENERATED_BODY()
    
protected:
	virtual void BeginPlay() override;
	
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pit")
	EPitGamePhases CurrentGamePhase = EPitGamePhases::Registration;
    
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pit")
	TArray<FName> ActiveButtonChannels;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pit")
	TMap<FName, int32> ChannelToPlayerIndex;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pit")
	TMap<int32, TObjectPtr<AActor>> PlayerActors;

	UFUNCTION()
	void HandleButtonPressed(FName Channel);
	
	UFUNCTION()
	void HandleButtonReleased(FName Channel);
	
	UFUNCTION()
	void HandleInputChanged(FName Channel, float Value, float Delta);
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Pit")
	void OnPlayerRegistered(int32 PlayerIndex, FName Channel);
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Pit")
	void OnPlayerAction(int32 PlayerIndex, FName Channel);
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Pit")
	void OnPlayerButtonReleased(int32 PlayerIndex, FName Channel);
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Pit")
	void OnPlayerInputChanged(int32 PlayerIndex, FName Channel, float Value, float Delta);
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Pit")
	void OnGamePhaseChanged(EPitGamePhases NewPhase);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pit|Spawning")
	TSubclassOf<AActor> PlayerPawnClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pit|Spawning")
	TSubclassOf<AActor> TrackActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pit|Spawning")
	bool bAutoSpawnOnRegister = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pit|Spawning")
	TArray<FTransform> PredefinedSpawnTransforms;

public:
	UFUNCTION(BlueprintCallable, Category = "Pit")
	void StartGame();
	
	UFUNCTION(BlueprintCallable, Category = "Pit")
	void AssignActorToPlayer(int32 PlayerIndex, AActor* TargetActor);
    
	UFUNCTION(BlueprintCallable, Category = "Pit")
	AActor* SpawnPlayerForIndex(int32 PlayerIndex);

	UFUNCTION(BlueprintPure, Category = "Pit")
	AActor* GetPlayerActor(int32 PlayerIndex) const;

	UFUNCTION(BlueprintPure, Category = "Pit")
	bool ResolvePlayerIndexAndSubChannel(FName InFullChannel, int32& OutPlayerIndex, FName& OutSubChannel) const;
};
