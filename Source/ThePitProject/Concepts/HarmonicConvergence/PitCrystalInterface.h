#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PitCrystalInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UPitCrystalInterface : public UInterface
{
	GENERATED_BODY()
};

class THEPITPROJECT_API IPitCrystalInterface
{
	GENERATED_BODY()

public:
	// Called continuously while a visitor charges this station (0.0 = dormant beneath floor, 1.0 = fully grown)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pit Crystal")
	void OnCrystalCharge(float ChargeProgress);

	// Called when this station reaches 100% charge and locks into the active drum looper
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pit Crystal")
	void OnCrystalActivated();

	// Called on synchronized 16th-note rhythm steps to pulse/flash the crystal
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pit Crystal")
	void OnCrystalBeatPulse(float PulseIntensity);

	// Called when the active loop lifespan expires and the crystal retreats back into the floor
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pit Crystal")
	void OnCrystalDeactivated();
};
