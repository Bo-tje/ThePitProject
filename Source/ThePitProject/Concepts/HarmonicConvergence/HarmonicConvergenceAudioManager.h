#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HarmonicConvergenceTypes.h"
#include "HarmonicStationActor.h"
#include "HarmonicConvergenceAudioManager.generated.h"

class UHarmonicConvergenceSynthComponent;
class UQuartzClockHandle;
class AHarmonicCrystalActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStationVoiceStarted, FName, StationChannelName, float, NoteFrequencyHz);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStationVoiceStopped, FName, StationChannelName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAttractPingTriggered, FName, StationChannelName, float, ChimeFrequencyHz);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPersistentBeatActivated, FName, StationChannelName, int32, BeatStep, float, NoteFrequencyHz);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPersistentBeatDeactivated, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHarmonicOverdriveStateChanged, bool, bIsOverdrive, float, OverdriveIntensity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCrystalEchoRippleTriggered, int32, StationIndex, FVector, WorldLocation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnStationBeatPulse, int32, StationIndex, float, PulseIntensity, FLinearColor, StationColor);

UCLASS(Blueprintable, BlueprintType)
class THEPITPROJECT_API AHarmonicConvergenceAudioManager : public AActor
{
	GENERATED_BODY()

public:
	AHarmonicConvergenceAudioManager();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// --- Game & Harmony Setup ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup")
	EHarmonicScaleMode ScaleMode = EHarmonicScaleMode::PentatonicMajor;

	// Placed AHarmonicStationActor instances in the level (auto-discovered on BeginPlay if left empty)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup")
	TArray<TObjectPtr<AHarmonicStationActor>> Stations;

	// --- Audio & Timing ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Audio")
	TObjectPtr<UHarmonicConvergenceSynthComponent> ProceduralSynthComponent;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Audio")
	TObjectPtr<UQuartzClockHandle> MasterQuartzClock;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Beat")
	float MasterBPM = 120.0f;

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Beat")
	void SetMasterBPM(float NewBPM);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Beat")
	int32 MaxActiveCrystals = 12;

	// Maximum active crystals allowed per physical station unit (4 buttons per station)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Beat", meta = (ClampMin = "1", ClampMax = "4"))
	int32 MaxActiveCrystalsPerStation = 2;

	// --- Attract & Overdrive ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Attract")
	bool bEnableAttractMode = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Attract")
	float AttractModeTimeoutSeconds = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Attract")
	float AttractChimeIntervalSeconds = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Overdrive")
	int32 OverdriveThreshold = 8;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Overdrive")
	float OverdriveIntensity = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Overdrive")
	bool bIsHarmonicOverdrive = false;

	// --- Events ---
	UPROPERTY(BlueprintAssignable, Category = "Harmonics|Events")
	FOnStationVoiceStarted OnStationVoiceStarted;

	UPROPERTY(BlueprintAssignable, Category = "Harmonics|Events")
	FOnStationVoiceStopped OnStationVoiceStopped;

	UPROPERTY(BlueprintAssignable, Category = "Harmonics|Events")
	FOnAttractPingTriggered OnAttractPingTriggered;

	UPROPERTY(BlueprintAssignable, Category = "Harmonics|Events")
	FOnPersistentBeatActivated OnPersistentBeatActivated;

	UPROPERTY(BlueprintAssignable, Category = "Harmonics|Events")
	FOnPersistentBeatDeactivated OnPersistentBeatDeactivated;

	UPROPERTY(BlueprintAssignable, Category = "Harmonics|Events")
	FOnHarmonicOverdriveStateChanged OnHarmonicOverdriveStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Harmonics|Events")
	FOnCrystalEchoRippleTriggered OnCrystalEchoRippleTriggered;

	UPROPERTY(BlueprintAssignable, Category = "Harmonics|Events")
	FOnStationBeatPulse OnStationBeatPulse;

	// --- Control & Queries ---
	UFUNCTION(BlueprintCallable, Category = "Harmonics|Control")
	void SetHarmonicScale(EHarmonicScaleMode NewScaleMode);

	UFUNCTION(BlueprintPure, Category = "Harmonics|Query")
	int32 GetActiveVoiceCount() const { return ActiveVoiceCount; }

	UFUNCTION(BlueprintPure, Category = "Harmonics|Query")
	float GetConvergenceEnergy() const { return ConvergenceEnergy; }

	UFUNCTION(BlueprintPure, Category = "Harmonics|Query")
	FVector GetCenterLocation() const { return GetActorLocation(); }

	UFUNCTION(BlueprintPure, Category = "Harmonics|Query")
	AHarmonicStationActor* FindStationByChannel(FName StationChannelName) const;

	UFUNCTION(BlueprintPure, Category = "Harmonics|Query")
	AHarmonicStationActor* FindStationByCrystal(const AHarmonicCrystalActor* Crystal) const;

	UFUNCTION(BlueprintPure, Category = "Harmonics|Overdrive")
	bool IsHarmonicOverdrive() const { return bIsHarmonicOverdrive; }

	UFUNCTION(BlueprintPure, Category = "Harmonics|Overdrive")
	float GetOverdriveIntensity() const { return OverdriveIntensity; }

	UFUNCTION(BlueprintPure, Category = "Harmonics|Harmony")
	int32 GetCurrentChordProgressionIndex() const;

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Beat")
	void ClearAllBeatSlots();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Runtime")
	int32 ActiveVoiceCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Runtime")
	float ConvergenceEnergy = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Runtime")
	float InactivityDurationSeconds = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Runtime")
	float TimeSinceLastAttractChimeSeconds = 0.0f;

private:
	void DiscoverStations();
	void ApplyHarmonicScaleFrequencies();
	void UpdateGameAndSound(float DeltaTime);
	void UpdateAttractMode(float DeltaTime, int32 ActiveLoopers);

	UFUNCTION()
	void HandleStationPressedChanged(AHarmonicStationActor* Station, bool bPressed);

	UFUNCTION()
	void HandleCrystalActivated(AHarmonicCrystalActor* Crystal);

	UFUNCTION()
	void HandleCrystalExpired(AHarmonicCrystalActor* Crystal);

	int32 LastBeatStep = -1;
};
