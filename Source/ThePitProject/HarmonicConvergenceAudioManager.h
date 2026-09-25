#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/AudioComponent.h"
#include "HarmonicConvergenceTypes.h"
#include "HarmonicConvergenceAudioManager.generated.h"

class UInputManagerSubSystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHarmonicCrescendoTriggered, float, Intensity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnShockwaveReleased, FName, Channel, float, HoldDuration);

UCLASS(Blueprintable, BlueprintType)
class THEPITPROJECT_API AHarmonicConvergenceAudioManager : public AActor
{
	GENERATED_BODY()

public:
	AHarmonicConvergenceAudioManager();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// --- Configurations ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup")
	EHarmonicScaleMode ScaleMode = EHarmonicScaleMode::PentatonicMajor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup")
	TArray<FStationVoiceConfig> StationConfigs;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Audio")
	TObjectPtr<UAudioComponent> CentralConvergenceAudioComponent;

	// Time in seconds to hold before releasing triggers the central ripple shockwave
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Parameters")
	float ShockwaveThresholdHoldTime = 5.0f;

	// All active hold time required to trigger the crescendo bloom
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Parameters")
	float CrescendoRequiredHoldTime = 3.0f;

	// Enable ambient wind-chimes attract mode when installation is idle
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|AttractMode")
	bool bEnableAttractMode = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|AttractMode")
	float AttractIdleThreshold = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|AttractMode")
	float AttractChimeInterval = 4.0f;

	// --- Delegates for Niagara/Visual synchronization ---
	UPROPERTY(BlueprintAssignable, Category = "Harmonics|Events")
	FOnHarmonicCrescendoTriggered OnHarmonicCrescendo;

	UPROPERTY(BlueprintAssignable, Category = "Harmonics|Events")
	FOnShockwaveReleased OnShockwaveReleased;

	// --- MetaSound & Musical Scale Control ---
	UFUNCTION(BlueprintCallable, Category = "Harmonics|Control")
	void SetHarmonicScale(EHarmonicScaleMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Control")
	void TriggerStationNoteOn(FName Channel);

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Control")
	void TriggerStationNoteOff(FName Channel);

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Control")
	void SetStationPressure(FName Channel, float Pressure);

	UFUNCTION(BlueprintPure, Category = "Harmonics|Query")
	int32 GetActiveVoiceCount() const { return ActiveVoiceCount; }

	UFUNCTION(BlueprintPure, Category = "Harmonics|Query")
	float GetConvergenceEnergy() const { return ConvergenceEnergy; }

	UFUNCTION(BlueprintPure, Category = "Harmonics|Query")
	bool GetVoiceState(FName Channel, FVoiceRuntimeState& OutState) const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Runtime")
	TMap<FName, FVoiceRuntimeState> VoiceStates;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Runtime")
	int32 ActiveVoiceCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Runtime")
	float ConvergenceEnergy = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Runtime")
	float AllStationsActiveTimer = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Runtime")
	float IdleTimer = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Runtime")
	float ChimeTimer = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Runtime")
	bool bCrescendoActive = false;

private:
	void InitializeDefaultStations();
	void ApplyHarmonicScaleFrequencies();
	void UpdateConvergenceParameters(float DeltaTime);
	void UpdateAttractMode(float DeltaTime);
	void PushMetaSoundParameters();

	UFUNCTION()
	void HandleOSCButtonPressed(FName Channel);

	UFUNCTION()
	void HandleOSCButtonReleased(FName Channel);

	UFUNCTION()
	void HandleOSCInputChanged(FName Channel, float Value, float Delta);

	UPROPERTY()
	TWeakObjectPtr<UInputManagerSubSystem> InputSubsystem;
};
