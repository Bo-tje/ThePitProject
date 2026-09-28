// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "PitControllableInterface.h"
#include "PitShootyPlayer.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShootyPlayerHealthChanged, float, NewHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShootyPlayerShieldToggled, bool, bIsActive);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnShootyPlayerFired);

/**
 * Player Pawn for the Shooty Game concept.
 * Supports lateral car movement along a defensive baseline, shooting projectiles forward,
 * and raising a shield to deflect incoming projectiles.
 */
UCLASS()
class THEPITPROJECT_API APitShootyPlayer : public APawn, public IPitControllableInterface
{
	GENERATED_BODY()

public:
	APitShootyPlayer();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// ------------------------------------------------------------------
	// Components
	// ------------------------------------------------------------------
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> CollisionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> CarMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ShieldMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> MuzzleLocation;

	// ------------------------------------------------------------------
	// Pit Controllable Interface
	// ------------------------------------------------------------------
	virtual void OnPlayerAssigned_Implementation(int32 PlayerIndex, FName Channel) override;
	virtual void OnActionPressed_Implementation(FName Channel) override;
	virtual void OnActionReleased_Implementation(FName Channel) override;
	virtual void OnAxisInput_Implementation(FName Channel, float Value, float Delta) override;

	// ------------------------------------------------------------------
	// Spline Movement & Tuning
	// ------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shooty|SplineMovement")
	TObjectPtr<AActor> TrackActor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shooty|SplineMovement")
	TObjectPtr<class USplineComponent> TargetSplineComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shooty|SplineMovement")
	float MovementSpeed = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shooty|SplineMovement")
	float CurrentDistance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shooty|SplineMovement")
	bool bUpdateRotationFromSpline = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shooty|SplineMovement")
	bool bFindTrackActorOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shooty|SplineMovement")
	TSubclassOf<AActor> TrackActorClass;

	// ------------------------------------------------------------------
	// Linear Fallback Movement (if no Spline is used)
	// ------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shooty|LinearMovement")
	float MinRailPosition = -500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shooty|LinearMovement")
	float MaxRailPosition = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shooty|LinearMovement")
	FVector MovementAxis = FVector(0.0f, 1.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shooty|Combat")
	TSubclassOf<AActor> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shooty|Combat")
	float FireCooldown = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shooty|Combat")
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shooty|Shield")
	float MaxShieldEnergy = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shooty|Shield")
	float ShieldDrainRate = 25.0f; // Energy per second while active

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shooty|Shield")
	float ShieldRechargeRate = 15.0f; // Energy per second while inactive

	// ------------------------------------------------------------------
	// State
	// ------------------------------------------------------------------
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shooty|State")
	int32 AssignedPlayerIndex = -1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shooty|State")
	FName AssignedChannel = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shooty|State")
	float CurrentPositionOnRail = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shooty|State")
	float CurrentHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shooty|State")
	float CurrentShieldEnergy = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shooty|State")
	bool bIsShieldActive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shooty|State")
	float LastFireTime = -100.0f;

	FVector InitialSpawnLocation;

public:
	// ------------------------------------------------------------------
	// Actions
	// ------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "Shooty|Actions")
	virtual void Fire();

	UFUNCTION(BlueprintCallable, Category = "Shooty|Actions")
	virtual void SetShieldActive(bool bActive);

	UFUNCTION(BlueprintCallable, Category = "Shooty|Actions")
	virtual void ApplyDamageToPlayer(float DamageAmount);

	UFUNCTION(BlueprintCallable, Category = "Shooty|Actions")
	virtual void MoveAlongSpline(float InputDelta);

	UFUNCTION(BlueprintCallable, Category = "Shooty|Actions")
	virtual void MoveAlongRail(float InputDelta);

	UFUNCTION(BlueprintCallable, Category = "Shooty|Actions")
	void SetTrackActor(AActor* NewTrackActor);

	UFUNCTION(BlueprintCallable, Category = "Shooty|Actions")
	void UpdateTransformOnSpline();

	// ------------------------------------------------------------------
	// Events / Delegates
	// ------------------------------------------------------------------
	UPROPERTY(BlueprintAssignable, Category = "Shooty|Events")
	FOnShootyPlayerHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Shooty|Events")
	FOnShootyPlayerShieldToggled OnShieldToggled;

	UPROPERTY(BlueprintAssignable, Category = "Shooty|Events")
	FOnShootyPlayerFired OnFired;
};
