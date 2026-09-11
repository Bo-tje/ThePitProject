#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OSCServer.h"
#include "InputManagerSubSystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnInputChanged, FName, Channel, float, Value, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChannelEvent, FName, Channel);

UCLASS()
class THEPITPROJECT_API UInputManagerSubSystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintAssignable, Category = "Input")
	FOnInputChanged OnInputChanged;

	UPROPERTY(BlueprintAssignable, Category = "Input")
	FOnChannelEvent OnButtonPressed;

	UPROPERTY(BlueprintAssignable, Category = "Input")
	FOnChannelEvent OnButtonReleased;

	UFUNCTION(BlueprintCallable, Category = "Input")
	void SetInputValue(FName Channel, float NewValue);

	UFUNCTION(BlueprintPure, Category = "Input")
	float GetInputValue(FName Channel) const;

	UFUNCTION(BlueprintPure, Category = "Input")
	bool IsInputActive(FName Channel) const;

	UFUNCTION(BlueprintCallable, Category = "OSC")
	bool StartOSCServer(const FString& InIPAddress = TEXT("0.0.0.0"), int32 InPort = 8000);

	UFUNCTION(BlueprintCallable, Category = "OSC")
	void StopOSCServer();

private:
	UPROPERTY()
	TMap<FName, float> InputStateCache;

	UPROPERTY()
	TObjectPtr<UOSCServer> OSCServer;

	void OnNativeOSCMessageReceived(const FOSCMessage& Message, const FString& IPAddress, uint16 Port);
};
