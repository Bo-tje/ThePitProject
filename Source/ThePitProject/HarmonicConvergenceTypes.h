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

USTRUCT(BlueprintType)
struct FStationVoiceConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice")
	FName ChannelName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice")
	float BaseFrequencyHz = 261.63f; // Default C4

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice")
	FLinearColor StreamColor = FLinearColor(0.0f, 0.9f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice")
	float PanPosition = 0.0f; // -1.0 Left to +1.0 Right

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice")
	FVector WorldStationLocation = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FVoiceRuntimeState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bIsPressed = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float CurrentPressure = 0.0f; // Continuous analog pressure if available

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float CurrentHoldDuration = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float ModulationIntensity = 0.0f; // 0.0 -> 1.0 based on hold curve

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float CurrentFrequency = 261.63f;
};
