#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/AudioComponent.h"
#include "HarmonicConvergenceTypes.h"
#include "PitCrystalInterface.h"
#include "HarmonicConvergenceAudioManager.generated.h"

class UInputManagerSubSystem;
class UHarmonicConvergenceSynthComponent;
class UNiagaraSystem;
class UNiagaraComponent;
class USceneComponent;
class UQuartzClockHandle;

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

// Event fired when Harmonic Overdrive status changes (collective climax / drop)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHarmonicOverdriveStateChanged, bool, bIsOverdrive, float, OverdriveIntensity);

// Event fired when an expiring crystal releases an outward Echo Ripple shockwave
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCrystalEchoRippleTriggered, int32, StationIndex, FVector, WorldLocation);

// Event fired when a station's charge progress changes (0.0 to 1.0) with its assigned color
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnStationChargeUpdated, int32, StationIndex, float, ChargeProgress, FLinearColor, StationColor);

// Event fired on synchronized rhythm steps for a station's crystal to pulse with its assigned color
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnStationBeatPulse, int32, StationIndex, float, PulseIntensity, FLinearColor, StationColor);

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

	// Radius of the central crystal core ring in centimeters (e.g. 85cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup")
	float CentralCrystalRingRadius = 85.0f;

	// Optional scene components (e.g. child Scene Components in Blueprint or Actor) to visually place each station's crystal
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup")
	TArray<TObjectPtr<USceneComponent>> CrystalPlacementPoints;

	// Optional pre-placed crystal Blueprint actors in the level (0..19). If assigned, beams and growth events route directly to each placed actor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup")
	TArray<TObjectPtr<AActor>> PlacedCrystalActors;

	// Base idle vortex swirl force when no players are active
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Vortex")
	float VortexBaseForce = 350.0f;

	// Peak particle spawn rate for the central vortex during convergence
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Vortex")
	float VortexMaxSpawnRate = 1800.0f;

	// Pure C++ Procedural Synth Component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Audio")
	TObjectPtr<UHarmonicConvergenceSynthComponent> ProceduralSynthComponent;

	// Sample-accurate master Quartz clock handle locking all stems in synchronized alignment
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Audio")
	TObjectPtr<UQuartzClockHandle> MasterQuartzClock;

	// Button hold duration in seconds required to charge and activate a persistent beat crystal shard
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Beat")
	float ButtonHoldThresholdSeconds = 2.0f;

	// Lifespan in seconds that an activated rhythmic beat crystal continues looping before decaying
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Beat")
	float BeatLifespanSeconds = 45.0f;

	// Tempo in beats per minute for the synchronized rhythmic 16-step sequencer (default 120 BPM)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Beat")
	float MasterBPM = 120.0f;

	// Updates the tempo (BPM) across both the Quartz stem clock and procedural beat sequencer
	UFUNCTION(BlueprintCallable, Category = "Harmonics|Beat")
	void SetMasterBPM(float NewBPM);

	// Maximum number of crystals that can be simultaneously active and looping (1..20, default 20 for full drum pad capability)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Beat")
	int32 MaxActiveCrystals = 20;

	// Whether attract mode is enabled when idle
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Attract")
	bool bEnableAttractMode = true;

	// Number of seconds of inactivity across all stations before attract mode begins chiming
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Attract")
	float AttractModeTimeoutSeconds = 15.0f;

	// Number of simultaneous active crystals required to detonate Harmonic Overdrive (The Drop)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Overdrive")
	int32 OverdriveThreshold = 8;

	// Current Overdrive climax intensity (0.0 = Normal, 1.0 = Full Drop Overdrive)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Overdrive")
	float OverdriveIntensity = 0.0f;

	// Whether the collective harmonic drop is actively detonating
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Overdrive")
	bool bIsHarmonicOverdrive = false;

	// Number of seconds between attract mode chimes while idle
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Attract")
	float AttractChimeIntervalSeconds = 4.0f;

	// Event fired when an individual station voice begins sounding
	UPROPERTY(BlueprintAssignable, Category = "Harmonics|Events")
	FOnStationVoiceStarted OnStationVoiceStarted;

	// Event fired when an individual station voice ends
	UPROPERTY(BlueprintAssignable, Category = "Harmonics|Events")
	FOnStationVoiceStopped OnStationVoiceStopped;

	// Event fired during idle attract mode to invite festival visitors
	UPROPERTY(BlueprintAssignable, Category = "Harmonics|Events")
	FOnAttractPingTriggered OnAttractPingTriggered;

	// Event fired when a visitor holds a station button and locks in a persistent beat shard
	UPROPERTY(BlueprintAssignable, Category = "Harmonics|Events")
	FOnPersistentBeatActivated OnPersistentBeatActivated;

	// Event fired when an active beat shard completes its lifespan and dissolves
	UPROPERTY(BlueprintAssignable, Category = "Harmonics|Events")
	FOnPersistentBeatDeactivated OnPersistentBeatDeactivated;

	// Event fired when Harmonic Overdrive status changes (drop climaxes / finishes)
	UPROPERTY(BlueprintAssignable, Category = "Harmonics|Events")
	FOnHarmonicOverdriveStateChanged OnHarmonicOverdriveStateChanged;

	// Fired when an expiring crystal releases an Echo Ripple shockwave to its neighbors
	UPROPERTY(BlueprintAssignable, Category = "Harmonics|Events")
	FOnCrystalEchoRippleTriggered OnCrystalEchoRippleTriggered;

	// Event fired continuously as a visitor charges a crystal (0.0 to 1.0)
	UPROPERTY(BlueprintAssignable, Category = "Harmonics|Events")
	FOnStationChargeUpdated OnStationChargeUpdated;

	// Event fired on rhythm steps to trigger pulse/glow animations on the crystal
	UPROPERTY(BlueprintAssignable, Category = "Harmonics|Events")
	FOnStationBeatPulse OnStationBeatPulse;

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
	bool GetStationConfig(FName StationChannelName, FStationVoiceConfig& OutConfig) const;

	UFUNCTION(BlueprintPure, Category = "Harmonics|Query")
	bool GetVoiceState(FName StationChannelName, FVoiceRuntimeState& OutState) const;

	UFUNCTION(BlueprintPure, Category = "Harmonics|Beat")
	const TArray<FPersistentBeatSlotInfo>& GetActiveBeatSlots() const { return PersistentBeatSlots; }

	UFUNCTION(BlueprintPure, Category = "Harmonics|Overdrive")
	bool IsHarmonicOverdrive() const { return bIsHarmonicOverdrive; }

	UFUNCTION(BlueprintPure, Category = "Harmonics|Overdrive")
	float GetOverdriveIntensity() const { return OverdriveIntensity; }

	UFUNCTION(BlueprintPure, Category = "Harmonics|Harmony")
	int32 GetCurrentChordProgressionIndex() const;

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Beat")
	void ClearAllBeatSlots();

	// Maps station index (0..19) to one of the 4 instrument families: 0=Bass, 1=Percussion, 2=Harmony, 3=Melody
	UFUNCTION(BlueprintPure, Category = "Harmonics|Beat")
	int32 GetStationSection(int32 StationIndex) const;

	// Returns the placed crystal actor assigned to this station (from PlacedCrystalActors or StationConfigs.CrystalTargetActor)
	UFUNCTION(BlueprintPure, Category = "Harmonics|Beat")
	AActor* GetStationCrystalActor(int32 StationIndex) const;

	// Calculates the 3D world location of a station's crystal shard (respects placement points, target actors, custom offsets, or default ring)
	UFUNCTION(BlueprintPure, Category = "Harmonics|Beat")
	FVector GetStationCrystalWorldLocation(int32 StationIndex) const;

	// Calculates the 3D world rotation of a station's crystal shard
	UFUNCTION(BlueprintPure, Category = "Harmonics|Beat")
	FRotator GetStationCrystalWorldRotation(int32 StationIndex) const;

	// Dynamically sets or repositions a station's crystal in the world at runtime
	UFUNCTION(BlueprintCallable, Category = "Harmonics|Beat")
	void SetStationCrystalLocation(int32 StationIndex, const FVector& NewWorldLocation, const FRotator& NewRotation = FRotator::ZeroRotator);

	// Dynamically sets or repositions a station's crystal relative to the manager actor
	UFUNCTION(BlueprintCallable, Category = "Harmonics|Beat")
	void SetStationCrystalRelativeLocation(int32 StationIndex, const FVector& NewRelativeLocation, const FRotator& NewRotation = FRotator::ZeroRotator);

	// Resets a station's crystal back to the central ring layout
	UFUNCTION(BlueprintCallable, Category = "Harmonics|Beat")
	void ResetStationCrystalToCenter(int32 StationIndex);

	// Convenience method to reposition all crystals along the railing perimeter near their respective button stations
	UFUNCTION(BlueprintCallable, Category = "Harmonics|Beat")
	void SetAllCrystalsToStationPerimeter(float InwardOffsetDistance = 60.0f);

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
