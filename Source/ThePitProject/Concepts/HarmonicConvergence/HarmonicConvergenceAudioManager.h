#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HarmonicConvergenceTypes.h"
#include "HarmonicStationActor.h"
#include "HarmonicConvergenceAudioManager.generated.h"

class UHarmonicConvergenceSynthComponent;
class AHarmonicCrystalActor;

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
	EHarmonicScaleMode ScaleMode = EHarmonicScaleMode::LydianCelestial;

	// Dynamically shifts ScaleMode along the brightness arc (Aeolian -> Dorian -> Lydian -> Ionian) based on active player/crystal count
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup")
	bool bAutoScaleMode = true;

	// Root note frequency in Hz (default 65.41 Hz = C2); all scale pitches are computed as semitone intervals from this root
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup", meta = (ClampMin = "32.0", ClampMax = "261.63"))
	float RootNoteFrequencyHz = 65.41f;

	// Granular intensity: starts at BaseActiveIntensity (0.5) on first active crystal/button and steps up by IntensityStepPerCrystal (0.20) up to 2.0
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup", meta = (ClampMin = "0.25", ClampMax = "1.5"))
	float BaseActiveIntensity = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float IntensityStepPerCrystal = 0.20f;

	// Placed AHarmonicStationActor instances in the level (auto-discovered on BeginPlay if left empty)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup")
	TArray<TObjectPtr<AHarmonicStationActor>> Stations;

	// --- Audio & Timing ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Audio")
	TObjectPtr<UHarmonicConvergenceSynthComponent> ProceduralSynthComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup")
	float MasterBPM = 120.0f;

	// Dynamically glides MasterBPM between MinDynamicBPM and MaxDynamicBPM based on active Scale Mode, rhythmic timbre drive, and crowd energy
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup")
	bool bEnableDynamicBPM = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup", meta = (ClampMin = "60.0", ClampMax = "140.0", EditCondition = "bEnableDynamicBPM"))
	float MinDynamicBPM = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup", meta = (ClampMin = "100.0", ClampMax = "180.0", EditCondition = "bEnableDynamicBPM"))
	float MaxDynamicBPM = 136.0f;

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Setup")
	void SetMasterBPM(float NewBPM);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup")
	int32 MaxActiveCrystals = 12;

	// Maximum active crystals allowed per physical station unit (4 buttons per station)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup", meta = (ClampMin = "1", ClampMax = "4"))
	int32 MaxActiveCrystalsPerStation = 2;

	// --- Hardware Sync Defaults (Applied to Stations/Crystals & ESP32s) ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup", meta = (ClampMin = "0.1"))
	float CrystalChargeTimeSeconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup", meta = (ClampMin = "1.0"))
	float CrystalActiveLifespanSeconds = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float TapCutMaxDurationSeconds = 0.40f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup", meta = (ClampMin = "0.0"))
	float StationBeamSpawnRate = 350.0f;

	// Fraction of StationBeamSpawnRate used as the trickle rate while a crystal is active
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StationTrickleMultiplier = 0.20f;

	// --- Vortex Force ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Vortex Force", meta = (ClampMin = "0.0"))
	float DefaultVortexForce = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Vortex Force", meta = (ClampMin = "0.0"))
	float TrickleVortexForce = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Vortex Force", meta = (ClampMin = "0.0"))
	float AeolianVortexForce = 1150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Vortex Force", meta = (ClampMin = "0.0"))
	float DorianVortexForce = 1450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Vortex Force", meta = (ClampMin = "0.0"))
	float LydianVortexForce = 1850.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Vortex Force", meta = (ClampMin = "0.0"))
	float IonianVortexForce = 2200.0f;

	UFUNCTION(BlueprintCallable, Category = "Harmonics|Setup")
	void SyncHardwareConfig();

	// --- Attract & Overdrive ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Attract")
	bool bEnableAttractMode = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Setup")
	float AttractModeTimeoutSeconds = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Attract")
	float AttractChimeIntervalSeconds = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Overdrive")
	int32 OverdriveThreshold = 8;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Overdrive")
	float OverdriveIntensity = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harmonics|Overdrive")
	bool bIsHarmonicOverdrive = false;

	// --- Modal Visual Theme Sync ---
	// How strongly the active Scale Mode atmosphere palette tints station Niagara beams & crystals (0 = original station colors, 1 = full thematic shift)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harmonics|Visuals", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VisualThemeBlendStrength = 0.55f;

	// --- Control & Queries ---
	UFUNCTION(BlueprintCallable, Category = "Harmonics|Control")
	void SetHarmonicScale(EHarmonicScaleMode NewScaleMode);

	UFUNCTION(BlueprintPure, Category = "Harmonics|Query")
	AHarmonicStationActor* FindStationByCrystal(const AHarmonicCrystalActor* Crystal) const;

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
	EChordVoiceRole GetEffectiveVoiceRole(const AHarmonicStationActor* Station) const;
	void UpdateGameAndSound(float DeltaTime);
	void UpdateAttractMode(float DeltaTime, int32 ActiveLoopers);
	void UpdateModalHarmonyAndVisuals(float DeltaTime, const int32 TimbreCounts[4], int32 CombinedActiveSources);
	FLinearColor GetModalAtmosphereColor(EHarmonicScaleMode Mode, int32 UnitIdx) const;

	UFUNCTION()
	void HandleStationPressedChanged(AHarmonicStationActor* Station, bool bPressed);

	UFUNCTION()
	void HandleCrystalActivated(AHarmonicCrystalActor* Crystal);

	UFUNCTION()
	void HandleCrystalExpired(AHarmonicCrystalActor* Crystal);

	int32 LastBeatStep = -1;
	float LastSyncedHardwareBPM = -1.0f;
	float LastCrystalActivationTimestamp = -10.0f;
	int32 LastActivatedStationIndex = INDEX_NONE;
};
