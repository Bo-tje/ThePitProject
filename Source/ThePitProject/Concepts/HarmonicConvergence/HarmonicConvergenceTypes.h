#pragma once

#include "CoreMinimal.h"
#include "HarmonicConvergenceTypes.generated.h"

UENUM(BlueprintType)
enum class EHarmonicScaleMode : uint8
{
	PentatonicMajor UMETA(DisplayName = "Pentatonic Major (C4, D4, E4, G4, A4, C5)"),
	PentatonicMinor UMETA(DisplayName = "Pentatonic Minor (C4, Eb4, F4, G4, Bb4, C5)"),
	LydianCelestial UMETA(DisplayName = "Lydian Celestial (C4, E4, F#4, G4, B4, D5)"),
	HirajoshiLuminous UMETA(DisplayName = "Hirajoshi Luminous (C4, Db4, F4, G4, Ab4, C5)"),
	DorianAmbient UMETA(DisplayName = "Dorian Ambient (C4, D4, Eb4, F4, G4, A4, Bb4)"),
	CustomFrequencies UMETA(DisplayName = "Custom Frequencies")
};

UENUM(BlueprintType)
enum class EVoiceTimbreProfile : uint8
{
	WarmPad UMETA(DisplayName = "Warm Analog Pad (Dual-Detuned Sustained)"),
	SubBassPad UMETA(DisplayName = "Deep Sub-Bass Anchor"),
	CrystallineChime UMETA(DisplayName = "Crystalline Chime / Glass Marimba Pluck"),
	VortexSweep UMETA(DisplayName = "Resonant Vortex Drone")
};

USTRUCT(BlueprintType)
struct FStationVoiceConfig
{
	GENERATED_BODY()

	// Hardware input OSC channel name (e.g. "player1", "player2")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station Config")
	FName ChannelName = NAME_None;

	// Musical base frequency in Hertz played when this station is triggered
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station Config")
	float BaseFrequencyHz = 261.63f; // Default C4

	// Synthesizer voice timbre character (WarmPad, SubBassPad, CrystallineChime, VortexSweep)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station Config")
	EVoiceTimbreProfile TimbreProfile = EVoiceTimbreProfile::WarmPad;

	// Visual beam stream color emitted toward center pit for Niagara particles
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station Config")
	FLinearColor StreamColor = FLinearColor(0.0f, 0.9f, 1.0f, 1.0f);

	// Stereo pan position across the railing perimeter (-1.0 = Far Left, 0.0 = Center, +1.0 = Far Right)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station Config")
	float PanPosition = 0.0f;

	// Physical 3D world location of this station's button console (origin for Niagara beam ribbons)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station Config")
	FVector WorldStationLocation = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FVoiceRuntimeState
{
	GENERATED_BODY()

	// Whether the station button is actively pressed / held down by a visitor
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voice State")
	bool bIsPressed = false;

	// Continuous analog pressure amount (normalized 0.0 to 1.0)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voice State")
	float CurrentPressure = 0.0f;

	// Cumulative duration in seconds that the button has been held down continuously
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voice State")
	float CurrentHoldDuration = 0.0f;

	// Dynamic modulation intensity (0.0 to 1.0) calculated from hold time curve and pressure
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voice State")
	float ModulationIntensity = 0.0f;

	// Current active pitch frequency in Hertz (including any vibrato or scale shifts)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voice State")
	float CurrentFrequency = 261.63f;
};
