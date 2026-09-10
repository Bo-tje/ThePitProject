// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OSCServer.h"
#include "InputManagerSubSystem.generated.h"

// Generic multi-purpose delegate for ANY button, dial, encoder, or sensor
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnInputChanged, FName, Channel, float, Value, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChannelEvent, FName, Channel);

/**
 * Infinitely scalable, input-agnostic Central Input Manager Subsystem for The Pit Project.
 * Automatically initializes on game launch, maintains state cache, and handles arbitrary input channels.
 */
UCLASS()
class THEPITPROJECT_API UInputManagerSubSystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Automatically called on game instance launch
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Automatically called on shutdown
	virtual void Deinitialize() override;

	// -------------------------------------------------------------
	// Global Event Dispatchers (Infinitely Scalable)
	// -------------------------------------------------------------

	/** Fires for ANY input change (buttons, rotary dials, sliders) with its normalized value and delta change */
	UPROPERTY(BlueprintAssignable, Category = "The Pit | Input")
	FOnInputChanged OnInputChanged;

	/** Convenience event fired whenever a digital input transitions to pressed (> 0.5) */
	UPROPERTY(BlueprintAssignable, Category = "The Pit | Input")
	FOnChannelEvent OnButtonPressed;

	/** Convenience event fired whenever a digital input transitions to released (<= 0.5) */
	UPROPERTY(BlueprintAssignable, Category = "The Pit | Input")
	FOnChannelEvent OnButtonReleased;

	// -------------------------------------------------------------
	// Generic Input Triggers (Callable from OSC, MIDI, Keys, or Blueprints)
	// -------------------------------------------------------------

	/** Update any input channel by name (e.g., Channel: "pit.corner.nw", Value: 1.0) */
	UFUNCTION(BlueprintCallable, Category = "The Pit | Input")
	void SetInputValue(FName Channel, float NewValue);

	/** Query the current state of ANY input channel at any time */
	UFUNCTION(BlueprintPure, Category = "The Pit | Input")
	float GetInputValue(FName Channel) const;

	/** Check if an input channel is currently active (> 0.5) */
	UFUNCTION(BlueprintPure, Category = "The Pit | Input")
	bool IsInputActive(FName Channel) const;

	// -------------------------------------------------------------
	// OSC Server Management
	// -------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "The Pit | OSC")
	bool StartOSCServer(const FString& InIPAddress = TEXT("0.0.0.0"), int32 InPort = 8000);

	UFUNCTION(BlueprintCallable, Category = "The Pit | OSC")
	void StopOSCServer();

private:
	// State memory storing the latest value of every connected channel
	UPROPERTY()
	TMap<FName, float> InputStateCache;

	UPROPERTY()
	TObjectPtr<UOSCServer> OSCServer;

	void OnNativeOSCMessageReceived(const FOSCMessage& Message, const FString& IPAddress, uint16 Port);
};
