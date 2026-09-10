// Fill out your copyright notice in the Description page of Project Settings.

#include "InputManagerSubSystem.h"
#include "OSCManager.h"

void UInputManagerSubSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("[The Pit] InputManagerSubSystem Initialized (Input-Agnostic Architecture)."));

	// Automatically start listening for OSC packets on port 8000
	StartOSCServer(TEXT("0.0.0.0"), 8000);
}

void UInputManagerSubSystem::Deinitialize()
{
	StopOSCServer();
	InputStateCache.Empty();
	UE_LOG(LogTemp, Log, TEXT("[The Pit] InputManagerSubSystem Deinitialized."));
	Super::Deinitialize();
}

void UInputManagerSubSystem::SetInputValue(FName Channel, float NewValue)
{
	float OldValue = 0.0f;
	if (const float* Found = InputStateCache.Find(Channel))
	{
		OldValue = *Found;
	}

	// Update cached state
	InputStateCache.Add(Channel, NewValue);
	const float Delta = NewValue - OldValue;

	// Broadcast generic change event to any listening Blueprint
	OnInputChanged.Broadcast(Channel, NewValue, Delta);

	// Digital button threshold events
	if (OldValue <= 0.5f && NewValue > 0.5f)
	{
		UE_LOG(LogTemp, Log, TEXT("[The Pit] Input Activated: %s (Value: %.2f)"), *Channel.ToString(), NewValue);
		OnButtonPressed.Broadcast(Channel);
	}
	else if (OldValue > 0.5f && NewValue <= 0.5f)
	{
		UE_LOG(LogTemp, Log, TEXT("[The Pit] Input Deactivated: %s"), *Channel.ToString());
		OnButtonReleased.Broadcast(Channel);
	}
}

float UInputManagerSubSystem::GetInputValue(FName Channel) const
{
	if (const float* Found = InputStateCache.Find(Channel))
	{
		return *Found;
	}
	return 0.0f;
}

bool UInputManagerSubSystem::IsInputActive(FName Channel) const
{
	return GetInputValue(Channel) > 0.5f;
}

bool UInputManagerSubSystem::StartOSCServer(const FString& InIPAddress, int32 InPort)
{
	StopOSCServer();

	OSCServer = UOSCManager::CreateOSCServer(InIPAddress, InPort, true, false, TEXT("PitInputServer"), this);
	if (OSCServer)
	{
		OSCServer->OnOscMessageReceivedNative.AddUObject(this, &UInputManagerSubSystem::OnNativeOSCMessageReceived);
		OSCServer->Listen();
		UE_LOG(LogTemp, Log, TEXT("[The Pit] Input-Agnostic OSC Server listening on %s:%d"), *InIPAddress, InPort);
		return true;
	}

	UE_LOG(LogTemp, Error, TEXT("[The Pit] Failed to start OSC Server on %s:%d"), *InIPAddress, InPort);
	return false;
}

void UInputManagerSubSystem::StopOSCServer()
{
	if (OSCServer)
	{
		OSCServer->Stop();
		OSCServer = nullptr;
		UE_LOG(LogTemp, Log, TEXT("[The Pit] OSC Server stopped."));
	}
}

void UInputManagerSubSystem::OnNativeOSCMessageReceived(const FOSCMessage& Message, const FString& IPAddress, uint16 Port)
{
	// Convert the OSC address pattern directly to a clean Channel FName
	const FOSCAddress Address = UOSCManager::GetOSCMessageAddress(Message);
	FString AddressStr = UOSCManager::GetOSCAddressFullPath(Address);

	// Normalize: "/pit/corner/nw" -> "pit.corner.nw"
	AddressStr.RemoveFromStart(TEXT("/"));
	AddressStr.ReplaceInline(TEXT("/"), TEXT("."));
	const FName Channel = FName(*AddressStr);

	float Value = 1.0f;
	TArray<float> Floats;
	UOSCManager::GetAllFloats(Message, Floats);
	if (Floats.Num() > 0)
	{
		Value = Floats[0];
	}

	SetInputValue(Channel, Value);
}
