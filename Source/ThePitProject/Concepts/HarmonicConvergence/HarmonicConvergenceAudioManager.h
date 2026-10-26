#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/AudioComponent.h"
#include "HarmonicConvergenceTypes.h"
#include "HarmonicConvergenceAudioManager.generated.h"

class UInputManagerSubSystem;
class UHarmonicConvergenceSynthComponent;
class UNiagaraSystem;
class UNiagaraComponent;

// Event fired when an individual station button is pressed and begins playing its musical tone
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStationVoiceStarted, FName, StationChannelName, float, NoteFrequencyHz);

// Event fired when an individual station button is released and its musical tone begins decaying
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStationVoiceStopped, FName, StationChannelName);

// Event fired during idle attract mode when a chime rings at a station to guide visitors
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAttractPingTriggered, FName, StationChannelName, float, ChimeFrequencyHz);

// Event fired when a station hold duration reaches threshold and activates a persistent rhythmic pulse beat
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPersistentBeatActivated, FName, StationChannelName, int32, BeatStep, float, NoteFrequencyHz);

// Event fired when a persistent rhythmic pulse beat finishes its lifespan and fades out
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPersistentBeatDeactivated, int32, SlotIndex);

// Persistent beat slot tracking struct
USTRUCT(BlueprintType)
struct FPersistentBeatSlotInfo
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Beat")
	FName StationChannelName = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Beat")
	int32 SlotIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Beat")
	int32 AssignedBeatStep = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Beat")
	float BaseFrequencyHz = 130.81f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Beat")
	EVoiceTimbreProfile TimbreProfile = EVoiceTimbreProfile::SubBassPad;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Beat")
	float PanPosition = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Beat")
	float RemainingLifespan = 45.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Beat")
	bool bIsActive = false;
};

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

	// Niagara system spawned per station to fire beams from WorldStationLocation toward CenterLocation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup")
	TObjectPtr<UNiagaraSystem> StationBeamSystem;

	// Central vortex Niagara system spawned at the manager's CenterLocation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup")
	TObjectPtr<UNiagaraSystem> CenterVortexSystem;

	// Pure C++ Procedural Synth Component (Zero MetaSound Nodes Required)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Audio")
	TObjectPtr<UHarmonicConvergenceSynthComponent> ProceduralSynthComponent;

	// Optional MetaSound Audio Component (for hybrid/fallback usage)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Audio")
	TObjectPtr<UAudioComponent> CentralConvergenceAudioComponent;

	// Enable ambient wind-chimes attract mode when installation is idle
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|AttractMode")
	bool bEnableAttractMode = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|AttractMode")
	float AttractIdleThreshold = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|AttractMode")
	float AttractChimeInterval = 4.0f;

	// --- Persistent Rhythmic Beat Settings ---
	// Hold duration required to activate a persistent looping beat slot (seconds)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Beat", meta = (ClampMin = "0.5", ClampMax = "10.0"))
	float HoldDurationToUnlockBeat = 1.75f;

	// Total lifespan of a persistent looping beat after button release (seconds)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Beat", meta = (ClampMin = "5.0", ClampMax = "300.0"))
	float BeatLifespanSeconds = 45.0f;

	// --- Delegates for Niagara/Visual synchronization ---

	// Fired when an individual station button is pressed and begins sounding
	UPROPERTY(BlueprintAssignable, Category = "Harmonics|Events")
	FOnStationVoiceStarted OnStationVoiceStarted;

	// Fired when an individual station button is released
	UPROPERTY(BlueprintAssignable, Category = "Harmonics|Events")
	FOnStationVoiceStopped OnStationVoiceStopped;

	// Fired during idle attract mode when a chime rings at a station to invite visitors
	UPROPERTY(BlueprintAssignable, Category = "Harmonics|Events")
	FOnAttractPingTriggered OnAttractPingTriggered;

	// Fired when a station hold duration reaches threshold and activates a persistent rhythmic pulse beat
	UPROPERTY(BlueprintAssignable, Category = "Harmonics|Events")
	FOnPersistentBeatActivated OnPersistentBeatActivated;

	// Fired when a persistent rhythmic pulse beat finishes its lifespan and fades out
	UPROPERTY(BlueprintAssignable, Category = "Harmonics|Events")
	FOnPersistentBeatDeactivated OnPersistentBeatDeactivated;

	// --- MetaSound & Musical Scale Control ---
	UFUNCTION(BlueprintCallable, Category = "Harmonics|Control")
	void SetHarmonicScale(EHarmonicScaleMode NewScaleMode);

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Control")
	void TriggerStationNoteOn(FName StationChannelName);

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Control")
	void TriggerStationNoteOff(FName StationChannelName);

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Control")
	void SetStationPressure(FName StationChannelName, float NormalizedPressure);

	UFUNCTION(BlueprintPure, Category = "Harmonics|Query")
	int32 GetActiveVoiceCount() const { return ActiveVoiceCount; }

	UFUNCTION(BlueprintPure, Category = "Harmonics|Query")
	float GetConvergenceEnergy() const { return ConvergenceEnergy; }

	UFUNCTION(BlueprintPure, Category = "Harmonics|Query")
	FVector GetCenterLocation() const { return GetActorLocation(); }

	UFUNCTION(BlueprintPure, Category = "Harmonics|Query")
	bool GetStationConfig(FName StationChannelName, FStationVoiceConfig& OutConfig) const;

	UFUNCTION(BlueprintPure, Category = "Harmonics|Query")
	bool GetVoiceState(FName StationChannelName, FVoiceRuntimeState& OutState) const;

	UFUNCTION(BlueprintPure, Category = "Harmonics|Beat")
	const TArray<FPersistentBeatSlotInfo>& GetActiveBeatSlots() const { return PersistentBeatSlots; }

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Beat")
	void ClearAllBeatSlots();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Runtime")
	TMap<FName, FVoiceRuntimeState> VoiceStates;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Runtime")
	TArray<FPersistentBeatSlotInfo> PersistentBeatSlots;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Runtime")
	int32 ActiveVoiceCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Runtime")
	float ConvergenceEnergy = 0.0f;

	// Elapsed time in seconds since any station button was pressed
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Runtime")
	float InactivityDurationSeconds = 0.0f;

	// Elapsed time in seconds since the last attract mode chime ping
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Runtime")
	float TimeSinceLastAttractChimeSeconds = 0.0f;

private:
	void InitializeDefaultStations();
	void ApplyHarmonicScaleFrequencies();
	void UpdateConvergenceParameters(float DeltaTime);
	void UpdateBeatSlots(float DeltaTime);
	void UpdateAttractMode(float DeltaTime);
	void PushMetaSoundParameters();
	void UpdateNiagaraParameters(float DeltaTime);

	UFUNCTION()
	void HandleOSCButtonPressed(FName StationChannelName);

	UFUNCTION()
	void HandleOSCButtonReleased(FName StationChannelName);

	UFUNCTION()
	void HandleOSCInputChanged(FName StationChannelName, float InputValue, float ValueDelta);

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UNiagaraComponent>> StationBeamComponents;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> CenterVortexComponent;

	FLinearColor CurrentCoreColor = FLinearColor(0.35f, 0.8f, 1.0f, 1.0f);

	UPROPERTY()
	TWeakObjectPtr<UInputManagerSubSystem> InputSubsystem;
};
