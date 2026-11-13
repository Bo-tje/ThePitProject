#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HarmonicCrystalActor.generated.h"

class USceneComponent;
class AHarmonicCrystalActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCrystalStateEvent, AHarmonicCrystalActor*, Crystal);

UCLASS(Blueprintable, BlueprintType)
class THEPITPROJECT_API AHarmonicCrystalActor : public AActor
{
	GENERATED_BODY()

public:
	AHarmonicCrystalActor();

	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crystal|Components")
	TObjectPtr<USceneComponent> CrystalRoot;

	// --- Charge & Lifespan Thresholds ---
	// When false, AHarmonicConvergenceAudioManager overrides ChargeThresholdSeconds at BeginPlay
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal|Behavior")
	bool bUseCustomChargeTime = false;

	// Seconds of continuous charging required to ignite and lock in this crystal
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal|Behavior", meta = (ClampMin = "0.1", EditCondition = "bUseCustomChargeTime"))
	float ChargeThresholdSeconds = 2.0f;

	// Charge rate multiplier applied while charging (1.0 = normal speed, higher = charges faster)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal|Behavior", meta = (ClampMin = "0.01"))
	float ChargeRate = 1.0f;

	// Seconds this crystal stays active and looping before expiring
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal|Behavior", meta = (ClampMin = "1.0"))
	float ActiveLifespanSeconds = 45.0f;

	// Interpolation speed at which uncharged or expired crystal energy decays back to 0
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crystal|Behavior", meta = (ClampMin = "0.01"))
	float ChargeDecayRate = 2.0f;

	// --- Runtime State ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crystal|Runtime")
	int32 StationIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crystal|Runtime")
	FLinearColor CrystalColor = FLinearColor(0.0f, 0.9f, 1.0f, 1.0f);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crystal|Runtime")
	bool bIsCharging = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crystal|Runtime")
	float ChargeProgress = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crystal|Runtime")
	bool bIsActive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crystal|Runtime")
	float RemainingLifespan = 45.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crystal|Runtime")
	float PulseFlash = 0.0f;

	bool bCanActivate = true;
	bool bLifespanDecaying = false;

	// --- C++ / Blueprint State Events ---
	UPROPERTY(BlueprintAssignable, Category = "Crystal|Events")
	FOnCrystalStateEvent OnCrystalActivatedEvent;

	UPROPERTY(BlueprintAssignable, Category = "Crystal|Events")
	FOnCrystalStateEvent OnCrystalExpiredEvent;

	// --- Blueprint Visual Hooks (for BP_Crystal) ---
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Crystal")
	void OnCrystalSetup(int32 InStationIndex, FLinearColor InStationColor);
	virtual void OnCrystalSetup_Implementation(int32 InStationIndex, FLinearColor InStationColor) {}

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Crystal")
	void OnCrystalCharge(float InChargeProgress, FLinearColor InStationColor);
	virtual void OnCrystalCharge_Implementation(float InChargeProgress, FLinearColor InStationColor) {}

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Crystal")
	void OnCrystalActivated(FLinearColor InStationColor);
	virtual void OnCrystalActivated_Implementation(FLinearColor InStationColor) {}

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Crystal")
	void OnCrystalBeatPulse(float PulseIntensity, FLinearColor InStationColor);
	virtual void OnCrystalBeatPulse_Implementation(float PulseIntensity, FLinearColor InStationColor) {}

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Crystal")
	void OnCrystalDeactivated();
	virtual void OnCrystalDeactivated_Implementation() {}

	// --- Control API ---
	UFUNCTION(BlueprintCallable, Category = "Crystal")
	void SetupCrystal(int32 InStationIndex, FLinearColor InColor);

	UFUNCTION(BlueprintCallable, Category = "Crystal")
	void SetCharging(bool bNewCharging);

	UFUNCTION(BlueprintCallable, Category = "Crystal")
	void TriggerBeatPulse(float Intensity = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Crystal")
	void DeactivateCrystal();

	UFUNCTION(BlueprintCallable, Category = "Crystal")
	void ResetCrystal();
};
