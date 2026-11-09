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
