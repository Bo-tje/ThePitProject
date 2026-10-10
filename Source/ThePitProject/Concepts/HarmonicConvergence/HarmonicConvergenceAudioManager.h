#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/AudioComponent.h"
#include "HarmonicConvergenceTypes.h"
#include "HarmonicConvergenceAudioManager.generated.h"

class UInputManagerSubSystem;
class UHarmonicConvergenceSynthComponent;



// Event fired when an individual station button is pressed and begins playing its musical tone
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStationVoiceStarted, FName, StationChannelName, float, NoteFrequencyHz);

// Event fired when an individual station button is released and its musical tone begins decaying
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStationVoiceStopped, FName, StationChannelName);

// Event fired during idle attract mode when a chime rings at a station to guide visitors
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAttractPingTriggered, FName, StationChannelName, float, ChimeFrequencyHz);

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
	bool GetVoiceState(FName StationChannelName, FVoiceRuntimeState& OutState) const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Runtime")
	TMap<FName, FVoiceRuntimeState> VoiceStates;

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
	void UpdateAttractMode(float DeltaTime);
	void PushMetaSoundParameters();

	UFUNCTION()
	void HandleOSCButtonPressed(FName StationChannelName);

	UFUNCTION()
	void HandleOSCButtonReleased(FName StationChannelName);

	UFUNCTION()
	void HandleOSCInputChanged(FName StationChannelName, float InputValue, float ValueDelta);

	UPROPERTY()
	TWeakObjectPtr<UInputManagerSubSystem> InputSubsystem;
};
