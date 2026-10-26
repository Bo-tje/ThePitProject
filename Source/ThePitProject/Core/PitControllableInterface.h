#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PitControllableInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UPitControllableInterface : public UInterface
{
	GENERATED_BODY()
};
    
class THEPITPROJECT_API IPitControllableInterface
{
	GENERATED_BODY()
    
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pit Control")
	void OnPlayerAssigned(int32 PlayerIndex, FName Channel);
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pit Control")
	void OnActionPressed(FName Channel);
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pit Control")
	void OnActionReleased(FName Channel);
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pit Control")
	void OnAxisInput(FName Channel, float Value, float Delta);
};
