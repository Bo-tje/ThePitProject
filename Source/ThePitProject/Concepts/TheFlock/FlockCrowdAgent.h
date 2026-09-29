#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "FlockCrowdAgent.generated.h"

UENUM(BlueprintType)
enum class EFlockAgentState : uint8
{
	IdleWander UMETA(DisplayName = "Idle Wander"),
	RunningToStation UMETA(DisplayName = "Running to Station"),
	CheeringAtRailing UMETA(DisplayName = "Cheering at Railing"),
	FlashMobDance UMETA(DisplayName = "Flash Mob Dance")
};

UCLASS()
class THEPITPROJECT_API AFlockCrowdAgent : public ACharacter
{
	GENERATED_BODY()

public:
	AFlockCrowdAgent();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Called by the Flock Manager when a railing station is active */
	UFUNCTION(BlueprintCallable, Category = "Flock")
	void SetTargetStationLocation(const FVector& NewTargetLocation, bool bIsStationActive);

	/** Trigger Flash Mob center dance */
	UFUNCTION(BlueprintCallable, Category = "Flock")
	void TriggerFlashMob(const FVector& CenterLocation);

	/** Reset back to ambient wandering */
	UFUNCTION(BlueprintCallable, Category = "Flock")
	void ReturnToIdle();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flock|State")
	EFlockAgentState CurrentState = EFlockAgentState::IdleWander;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flock|Speed")
	float WalkSpeed = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flock|Speed")
	float SprintSpeed = 650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flock|Animation")
	TObjectPtr<UAnimMontage> CheerMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flock|Animation")
	TObjectPtr<UAnimMontage> DanceMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flock|Bounds")
	float ArrivalDistance = 120.0f;

private:
	FVector CurrentTargetLocation = FVector::ZeroVector;
	bool bHasActiveTarget = false;
	float IdleWanderTimer = 0.0f;

	void UpdateMovementLogic(float DeltaTime);
	void PickRandomWanderPoint();
};
