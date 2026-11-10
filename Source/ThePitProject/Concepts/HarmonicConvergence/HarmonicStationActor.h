#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HarmonicConvergenceTypes.h"
#include "HarmonicCrystalActor.h"
#include "HarmonicStationActor.generated.h"

class USceneComponent;
class UNiagaraSystem;
class UNiagaraComponent;
class UAudioComponent;
class USoundWave;
class UQuartzClockHandle;
class AHarmonicStationActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStationPressedChanged, AHarmonicStationActor*, Station, bool, bPressed);

UCLASS(Blueprintable, BlueprintType)
class THEPITPROJECT_API AHarmonicStationActor : public AActor
{
	GENERATED_BODY()

public:
	AHarmonicStationActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Station|Components")
	TObjectPtr<USceneComponent> StationRoot;

	// --- Station Configuration ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station|Config", meta = (ClampMin = "0", ClampMax = "23"))
	int32 StationIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station|Config")
	FName ChannelName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station|Config")
	FLinearColor StreamColor = FLinearColor(0.0f, 0.9f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station|Config")
	EVoiceTimbreProfile TimbreProfile = EVoiceTimbreProfile::SubBassPad;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station|Config")
	float BaseFrequencyHz = 261.63f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station|Config", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float PanPosition = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station|Config")
	int32 RhythmicPatternMask = 0x1111;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station|Config")
	TObjectPtr<USoundWave> AudioStemAsset = nullptr;

	// Linked AHarmonicCrystalActor in the scene
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station|Config")
	TObjectPtr<AHarmonicCrystalActor> CrystalActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station|Config")
	TObjectPtr<UNiagaraSystem> StationBeamSystem = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station|Config")
	float SpawnRate = 350.0f;

	// Maximum press duration to register as a "Tap to Cut" deactivation on an already-active crystal
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station|Config", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float TapCutMaxDurationSeconds = 0.40f;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Station|Config")
	void ApplyDefaultPresetForIndex();

	// --- Runtime State ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Station|Runtime")
	bool bIsPressed = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Station|Runtime")
	float LastPressTimestamp = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Station|Runtime")
	float CurrentPressure = 0.0f;

	UPROPERTY(BlueprintAssignable, Category = "Station|Events")
	FOnStationPressedChanged OnStationPressedChanged;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> BeamComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> StemAudioComponent;

	UFUNCTION(BlueprintCallable, Category = "Station")
	void SetPressed(bool bNewPressed);

	UFUNCTION(BlueprintPure, Category = "Station")
	FVector GetCrystalWorldLocation() const;

	UFUNCTION(BlueprintPure, Category = "Station")
	float GetChargeProgress() const { return CrystalActor ? CrystalActor->ChargeProgress : 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Station")
	bool IsCrystalActive() const { return CrystalActor && CrystalActor->bIsActive; }

	void InitializeAudioStem(UQuartzClockHandle* MasterClock);
	void UpdateBeamAndStem(float DeltaTime, float ConvergenceEnergy, int32 ActiveStationCount, float CoreBrightness);
	void ResetStation();

private:
	UFUNCTION()
	void HandleOSCButtonPressed(FName InChannel);

	UFUNCTION()
	void HandleOSCButtonReleased(FName InChannel);

	UFUNCTION()
	void HandleOSCInputChanged(FName InChannel, float InputValue, float ValueDelta);
};
