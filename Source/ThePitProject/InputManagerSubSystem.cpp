#include "InputManagerSubSystem.h"
#include "OSCManager.h"

void UInputManagerSubSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	StartOSCServer(TEXT("0.0.0.0"), 8000);
}

void UInputManagerSubSystem::Deinitialize()
{
	StopOSCServer();
	InputStateCache.Empty();
	Super::Deinitialize();
}

void UInputManagerSubSystem::SetInputValue(FName Channel, float NewValue)
{
	float& StoredValue = InputStateCache.FindOrAdd(Channel, 0.0f);
	const float OldValue = StoredValue;
	StoredValue = NewValue;

	const float Delta = NewValue - OldValue;
	OnInputChanged.Broadcast(Channel, NewValue, Delta);

	if (OldValue <= 0.5f && NewValue > 0.5f)
	{
		OnButtonPressed.Broadcast(Channel);
	}
	else if (OldValue > 0.5f && NewValue <= 0.5f)
	{
		OnButtonReleased.Broadcast(Channel);
	}
}

float UInputManagerSubSystem::GetInputValue(FName Channel) const
{
	const float* Found = InputStateCache.Find(Channel);
	return Found ? *Found : 0.0f;
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
		return true;
	}
	return false;
}

void UInputManagerSubSystem::StopOSCServer()
{
	if (OSCServer)
	{
		OSCServer->Stop();
		OSCServer = nullptr;
	}
}

void UInputManagerSubSystem::OnNativeOSCMessageReceived(const FOSCMessage& Message, const FString& IPAddress, uint16 Port)
{
	FString Address = UOSCManager::GetOSCAddressFullPath(UOSCManager::GetOSCMessageAddress(Message));
	Address.RemoveFromStart(TEXT("/"));
	Address.ReplaceInline(TEXT("/"), TEXT("."));

	float Value = 1.0f;
	TArray<float> Floats;
	UOSCManager::GetAllFloats(Message, Floats);
	if (Floats.Num() > 0)
	{
		Value = Floats[0];
	}

	SetInputValue(FName(*Address), Value);
}
