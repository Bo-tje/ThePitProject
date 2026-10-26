#include "PitPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputManagerSubSystem.h"
#include "Engine/LocalPlayer.h"

APitPlayerController::APitPlayerController()
{
	bAutoManageActiveCameraTarget = true;
}

void APitPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		// Default assigned player index to the local controller ID if available
		AssignedPlayerIndex = LocalPlayer->GetControllerId();

		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

void APitPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		for (const FPitActionChannelMapping& Mapping : ActionMappings)
		{
			if (Mapping.InputAction && !Mapping.SubChannel.IsNone())
			{
				// Triggered handles continuous axis values and button presses
				EnhancedInputComponent->BindAction(
					Mapping.InputAction,
					ETriggerEvent::Triggered,
					this,
					&APitPlayerController::HandleActionTriggered,
					Mapping.SubChannel
				);

				// Completed sends 0.0 when button is released or axis neutralizes
				EnhancedInputComponent->BindAction(
					Mapping.InputAction,
					ETriggerEvent::Completed,
					this,
					&APitPlayerController::HandleActionCompleted,
					Mapping.SubChannel
				);
			}
		}
	}
}

void APitPlayerController::HandleActionTriggered(const FInputActionValue& Value, FName SubChannel)
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UInputManagerSubSystem* InputSubsystem = GameInstance->GetSubsystem<UInputManagerSubSystem>())
		{
			const float FloatValue = Value.Get<float>();
			const FName FullChannel = *FString::Printf(TEXT("player%d.%s"), AssignedPlayerIndex, *SubChannel.ToString());
			InputSubsystem->SetInputValue(FullChannel, FloatValue);
		}
	}
}

void APitPlayerController::HandleActionCompleted(FName SubChannel)
{
	HandleActionTriggered(FInputActionValue(0.0f), SubChannel);
}

void APitPlayerController::SetAssignedPlayerIndex(int32 NewPlayerIndex)
{
	AssignedPlayerIndex = NewPlayerIndex;
}
