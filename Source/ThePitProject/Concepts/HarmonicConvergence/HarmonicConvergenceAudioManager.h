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
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

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

// Persistent beat slot tracking struct (4 section slots: 0=Bass, 1=Percussion, 2=Harmony, 3=Melody)
USTRUCT(BlueprintType)
struct FPersistentBeatSlotInfo
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Beat")
	FName StationChannelName = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Beat")
	int32 StationIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Beat")
	int32 SlotIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Beat")
	int32 RhythmicPatternMask = 0x1111;

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

	// Niagara system spawned per station to fire beams from WorldStationLocation toward its central crystal
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup")
	TObjectPtr<UNiagaraSystem> StationBeamSystem;

	// Niagara system for the 20 crystals forming the central core ring
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup")
	TObjectPtr<UNiagaraSystem> BeatCrystalSystem;

	// Optional Static Mesh asset to represent the physical crystal shards in the center (when 3D assets are provided)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup")
	TObjectPtr<UStaticMesh> CrystalMeshAsset;

	// Optional Material for the crystal static meshes (dynamic material instances created automatically)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup")
	TObjectPtr<UMaterialInterface> CrystalBaseMaterial;

	// Scale factor for the central crystal static meshes
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup")
	FVector CrystalMeshScale = FVector(0.35f, 0.35f, 0.75f);

	// Radius of the central crystal core ring in centimeters (e.g. 85cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup")
	float CentralCrystalRingRadius = 85.0f;

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
	// Hold duration required to power up and lock in a persistent looping beat slot (seconds, 0.75s default for snappy drum pad feel)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Beat", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float HoldDurationToUnlockBeat = 0.75f;

	// Maximum number of crystals that can be simultaneously active and looping (1..20, default 20 for full drum pad capability)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Beat", meta = (ClampMin = "1", ClampMax = "20"))
	int32 MaxActiveCrystals = 20;

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

	// Maps station index (0..19) to one of the 4 instrument families: 0=Bass, 1=Percussion, 2=Harmony, 3=Melody
	UFUNCTION(BlueprintPure, Category = "Harmonics|Beat")
	int32 GetStationSection(int32 StationIndex) const;

	// Calculates the 3D world location of a station's crystal shard in the central ring
	UFUNCTION(BlueprintPure, Category = "Harmonics|Beat")
	FVector GetStationCrystalWorldLocation(int32 StationIndex) const;

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

	// Per-station crystal shard Niagara components in the central circular core
	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> StationCrystalComponents;

	// Optional physical crystal StaticMesh components
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> StationCrystalMeshComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> StationCrystalDynamicMaterials;

	// Synchronized audio components for optional pre-rendered WAV SoundWave stems
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> StationStemAudioComponents;

	float StationPulseFlash[20] = {0.0f};
	float StationBeatPulse[20] = {0.0f};
	float StationChargeProgress[20] = {0.0f};
	int32 LastBeatStep = -1;

	UPROPERTY()
	TWeakObjectPtr<UInputManagerSubSystem> InputSubsystem;
};
