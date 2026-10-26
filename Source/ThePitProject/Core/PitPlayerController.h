#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "PitPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;

USTRUCT(BlueprintType)
struct FPitActionChannelMapping
{
	GENERATED_BODY()

	/** The Enhanced Input Action asset (e.g., IA_Move, IA_TriggerAction) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pit Input")
	TObjectPtr<const UInputAction> InputAction = nullptr;

	/** The subchannel name sent to the subsystem (e.g. "move", "steer", "action", "shoot") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pit Input")
	FName SubChannel = NAME_None;
};

/**
 * Generic Player Controller that maps Enhanced Input Actions
 * into the UInputManagerSubSystem using player-indexed channels (e.g. "player0.move", "player0.action").
 */
UCLASS()
class THEPITPROJECT_API APitPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	APitPlayerController();

	/** Change the assigned player index on the fly during testing */
	UFUNCTION(BlueprintCallable, Category = "Pit Input")
	void SetAssignedPlayerIndex(int32 NewPlayerIndex);

	UFUNCTION(BlueprintPure, Category = "Pit Input")
	int32 GetAssignedPlayerIndex() const { return AssignedPlayerIndex; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	/** Default Input Mapping Context to apply to this local player */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pit Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** Generic list of Input Actions mapped to their target subchannel name */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pit Input")
	TArray<FPitActionChannelMapping> ActionMappings;

	/** Player index for this controller (defaults to LocalPlayer ControllerId or 0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pit Input")
	int32 AssignedPlayerIndex = 0;

private:
	void HandleActionTriggered(const FInputActionValue& Value, FName SubChannel);
	void HandleActionCompleted(FName SubChannel);
};
