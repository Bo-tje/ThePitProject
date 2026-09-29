#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FlockCrowdAgent.h"
#include "FlockCrowdManager.generated.h"

class UInputManagerSubSystem;

UCLASS()
class THEPITPROJECT_API AFlockCrowdManager : public AActor
{
	GENERATED_BODY()

public:
	AFlockCrowdManager();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Spawns the flock crowd across the pit floor */
	UFUNCTION(BlueprintCallable, Category = "Flock|Manager")
	void SpawnFlockCrowd();

	/** Triggered when a specific station button state changes */
	UFUNCTION(BlueprintCallable, Category = "Flock|Manager")
	void OnStationInputChanged(int32 StationIndex, bool bIsPressed);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flock|Setup")
	TSubclassOf<AFlockCrowdAgent> CrowdAgentClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flock|Setup")
	int32 CrowdSize = 40;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flock|Setup")
	FVector PitCenterLocation = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flock|Setup")
	FVector SpawnAreaExtents = FVector(1000.0f, 800.0f, 0.0f);

	/** The 4 Railing Stations */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flock|Stations")
	FVector StationWestLocation = FVector(-1200.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flock|Stations")
	FVector StationNorthLocation = FVector(0.0f, 1000.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flock|Stations")
	FVector StationEastLocation = FVector(1200.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flock|Stations")
	FVector StationSouthLocation = FVector(0.0f, -1000.0f, 0.0f);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flock|Agents")
	TArray<TObjectPtr<AFlockCrowdAgent>> ActiveAgents;

private:
	UPROPERTY()
	TObjectPtr<UInputManagerSubSystem> InputSubsystem;

	TArray<bool> StationActiveStates;

	UFUNCTION()
	void HandleInputChanged(FName Channel, float Value, float Delta);

	void UpdateFlockBehavior();
};
