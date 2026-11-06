#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PitCrystalInterface.generated.h"

UINTERFACE(BlueprintType, Blueprintable)
class THEPITPROJECT_API UPitCrystalInterface : public UInterface
{
	GENERATED_BODY()
};

class THEPITPROJECT_API IPitCrystalInterface
{
	GENERATED_BODY()

public:
	// Called on BeginPlay to initialize the crystal with its station index and unique visual color
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Pit Crystal")
	void OnCrystalSetup(int32 StationIndex, FLinearColor StationColor);
	virtual void OnCrystalSetup_Implementation(int32 StationIndex, FLinearColor StationColor) {}

	// Called continuously as crystal charge changes up or down (0.0 = dormant beneath floor, 1.0 = fully grown)
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Pit Crystal")
	void OnCrystalCharge(float ChargeProgress, FLinearColor StationColor);
	virtual void OnCrystalCharge_Implementation(float ChargeProgress, FLinearColor StationColor) {}

	// Called when this station reaches 100% charge and locks into the active drum looper
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Pit Crystal")
	void OnCrystalActivated(FLinearColor StationColor);
	virtual void OnCrystalActivated_Implementation(FLinearColor StationColor) {}

	// Called on synchronized 16th-note rhythm steps to pulse/flash the crystal
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Pit Crystal")
	void OnCrystalBeatPulse(float PulseIntensity, FLinearColor StationColor);
	virtual void OnCrystalBeatPulse_Implementation(float PulseIntensity, FLinearColor StationColor) {}

	// Called when the active loop lifespan expires and the crystal retreats back into the floor
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Pit Crystal")
	void OnCrystalDeactivated();
	virtual void OnCrystalDeactivated_Implementation() {}
};
