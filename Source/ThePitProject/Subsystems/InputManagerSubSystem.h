#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OSCServer.h"
#include "OSCClient.h"
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

	// Server management (Incoming OSC)
	UFUNCTION(BlueprintCallable, Category = "OSC|Server")
	bool StartOSCServer(const FString& InIPAddress = TEXT("0.0.0.0"), int32 InPort = 8000);

	UFUNCTION(BlueprintCallable, Category = "OSC|Server")
	void StopOSCServer();

	// Client management (Outgoing OSC)
	UFUNCTION(BlueprintCallable, Category = "OSC|Client")
	bool StartOSCClient(const FString& InIPAddress = TEXT("127.0.0.1"), int32 InPort = 8888);

	UFUNCTION(BlueprintCallable, Category = "OSC|Client")
	void StopOSCClient();

	UFUNCTION(BlueprintCallable, Category = "OSC|Client")
	void SetClientEndpoint(const FString& InIPAddress, int32 InPort);

	// Generic OSC Senders
	UFUNCTION(BlueprintCallable, Category = "OSC|Sender")
	void SendOSCFloat(const FString& Address, float Value);

	UFUNCTION(BlueprintCallable, Category = "OSC|Sender")
	void SendOSCInt(const FString& Address, int32 Value);

	UFUNCTION(BlueprintCallable, Category = "OSC|Sender")
	void SendOSCBool(const FString& Address, bool Value);

	UFUNCTION(BlueprintCallable, Category = "OSC|Sender")
	void SendOSCString(const FString& Address, const FString& Value);

	// Dedicated LED & Hardware Output functions
	UFUNCTION(BlueprintCallable, Category = "OSC|Lighting")
	void SendLEDState(FName Channel, float Value);

	UFUNCTION(BlueprintCallable, Category = "OSC|Lighting")
	void SendAttractState(bool bEnableAttract);

	UFUNCTION(BlueprintCallable, Category = "OSC|Lighting")
	void SendAllLEDs(float Value);

private:
	UPROPERTY()
	TMap<FName, float> InputStateCache;

	UPROPERTY()
	TObjectPtr<UOSCServer> OSCServer;

	UPROPERTY()
	TObjectPtr<UOSCClient> OSCClient;

	UPROPERTY(EditAnywhere, Category = "OSC|Client")
	FString TargetClientIP = TEXT("127.0.0.1");

	UPROPERTY(EditAnywhere, Category = "OSC|Client")
	int32 TargetClientPort = 8888;

	void OnNativeOSCMessageReceived(const FOSCMessage& Message, const FString& IPAddress, uint16 Port);
	static FOSCAddress FormatOSCAddress(const FString& InAddress);
};
