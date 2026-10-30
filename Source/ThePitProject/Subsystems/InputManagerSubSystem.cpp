#include "InputManagerSubSystem.h"
#include "OSCManager.h"
#include "OSCAddress.h"
#include "OSCMessage.h"
#include "OSCClient.h"

void UInputManagerSubSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	StartOSCServer(TEXT("0.0.0.0"), 8000);
	StartOSCClient(TargetClientIP, TargetClientPort);
}

void UInputManagerSubSystem::Deinitialize()
{
	StopOSCServer();
	StopOSCClient();
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

bool UInputManagerSubSystem::StartOSCClient(const FString& InIPAddress, int32 InPort)
{
	StopOSCClient();

	TargetClientIP = InIPAddress.IsEmpty() ? TEXT("127.0.0.1") : InIPAddress;
	TargetClientPort = InPort > 0 ? InPort : 8888;

	OSCClient = UOSCManager::CreateOSCClient(TargetClientIP, TargetClientPort, TEXT("PitOutputClient"), this);
	if (OSCClient)
	{
		if (!OSCClient->IsActive())
		{
			OSCClient->Connect();
		}
		return true;
	}
	return false;
}

void UInputManagerSubSystem::StopOSCClient()
{
	if (OSCClient)
	{
		OSCClient = nullptr;
	}
}

void UInputManagerSubSystem::SetClientEndpoint(const FString& InIPAddress, int32 InPort)
{
	TargetClientIP = InIPAddress;
	TargetClientPort = InPort;

	if (OSCClient)
	{
		OSCClient->SetSendIPAddress(TargetClientIP, TargetClientPort);
	}
	else
	{
		StartOSCClient(TargetClientIP, TargetClientPort);
	}
}

FOSCAddress UInputManagerSubSystem::FormatOSCAddress(const FString& InAddress)
{
	FString Formatted = InAddress;
	Formatted.ReplaceInline(TEXT("."), TEXT("/"));
	if (!Formatted.StartsWith(TEXT("/")))
	{
		Formatted = TEXT("/") + Formatted;
	}
	return UOSCManager::ConvertStringToOSCAddress(Formatted);
}

void UInputManagerSubSystem::SendOSCFloat(const FString& Address, float Value)
{
	if (!OSCClient)
	{
		StartOSCClient(TargetClientIP, TargetClientPort);
	}

	if (OSCClient)
	{
		FOSCMessage Message;
		UOSCManager::SetOSCMessageAddress(Message, FormatOSCAddress(Address));
		UOSCManager::AddFloat(Message, Value);
		OSCClient->SendOSCMessage(Message);
	}
}

void UInputManagerSubSystem::SendOSCInt(const FString& Address, int32 Value)
{
	if (!OSCClient)
	{
		StartOSCClient(TargetClientIP, TargetClientPort);
	}

	if (OSCClient)
	{
		FOSCMessage Message;
		UOSCManager::SetOSCMessageAddress(Message, FormatOSCAddress(Address));
		UOSCManager::AddInt32(Message, Value);
		OSCClient->SendOSCMessage(Message);
	}
}

void UInputManagerSubSystem::SendOSCBool(const FString& Address, bool Value)
{
	if (!OSCClient)
	{
		StartOSCClient(TargetClientIP, TargetClientPort);
	}

	if (OSCClient)
	{
		FOSCMessage Message;
		UOSCManager::SetOSCMessageAddress(Message, FormatOSCAddress(Address));
		UOSCManager::AddBool(Message, Value);
		OSCClient->SendOSCMessage(Message);
	}
}

void UInputManagerSubSystem::SendOSCString(const FString& Address, const FString& Value)
{
	if (!OSCClient)
	{
		StartOSCClient(TargetClientIP, TargetClientPort);
	}

	if (OSCClient)
	{
		FOSCMessage Message;
		UOSCManager::SetOSCMessageAddress(Message, FormatOSCAddress(Address));
		FString StrVal = Value;
		UOSCManager::AddString(Message, StrVal);
		OSCClient->SendOSCMessage(Message);
	}
}

void UInputManagerSubSystem::SendACK(const FString& InIPAddress, int32 SequenceId, int32 InPort)
{
	const FString TargetIP = InIPAddress.IsEmpty() ? TargetClientIP : InIPAddress;
	const int32 TargetPort = InPort > 0 ? InPort : TargetClientPort;

	if (!OSCClient)
	{
		StartOSCClient(TargetIP, TargetPort);
	}
	else
	{
		OSCClient->SetSendIPAddress(TargetIP, TargetPort);
	}

	if (OSCClient)
	{
		FOSCMessage AckMsg;
		UOSCManager::SetOSCMessageAddress(AckMsg, FormatOSCAddress(TEXT("/pit/ack")));
		UOSCManager::AddInt32(AckMsg, SequenceId);
		OSCClient->SendOSCMessage(AckMsg);
	}
}

void UInputManagerSubSystem::SendLEDState(FName Channel, float Value)
{
	FString ChannelStr = Channel.ToString();
	FString Address;
	if (ChannelStr.StartsWith(TEXT("pit.")))
	{
		ChannelStr.RemoveFromStart(TEXT("pit."));
		Address = FString::Printf(TEXT("/pit/led/%s"), *ChannelStr.Replace(TEXT("."), TEXT("/")));
	}
	else if (ChannelStr.StartsWith(TEXT("/pit/")))
	{
		ChannelStr.RemoveFromStart(TEXT("/pit/"));
		Address = FString::Printf(TEXT("/pit/led/%s"), *ChannelStr);
	}
	else
	{
		Address = FString::Printf(TEXT("/pit/led/%s"), *ChannelStr.Replace(TEXT("."), TEXT("/")));
	}

	SendOSCFloat(Address, Value);
}

void UInputManagerSubSystem::SendAttractState(bool bEnableAttract)
{
	SendOSCFloat(TEXT("/pit/attract"), bEnableAttract ? 1.0f : 0.0f);
	SendOSCBool(TEXT("/pit/led/attract"), bEnableAttract);
}

void UInputManagerSubSystem::SendAllLEDs(float Value)
{
	SendOSCFloat(TEXT("/pit/led/all"), Value);
}

void UInputManagerSubSystem::OnNativeOSCMessageReceived(const FOSCMessage& Message, const FString& IPAddress, uint16 Port)
{
	FString FullPath = UOSCManager::GetOSCAddressFullPath(UOSCManager::GetOSCMessageAddress(Message));

	// 1. Reliable Unit Snapshot Handler (/pit/unit<UnitId>/inputs)
	// Payload format: [int32 sequenceId, float b0, float b1, ...]
	if (FullPath.StartsWith(TEXT("/pit/unit")) && FullPath.EndsWith(TEXT("/inputs")))
	{
		int32 SequenceId = 0;
		UOSCManager::GetInt32(Message, 0, SequenceId);

		FString UnitStr = FullPath;
		UnitStr.RemoveFromStart(TEXT("/pit/unit"));
		UnitStr.RemoveFromEnd(TEXT("/inputs"));
		int32 UnitId = FCString::Atoi(*UnitStr);
		if (UnitId < 1)
		{
			UnitId = 1;
		}

		TArray<float> ButtonStates;
		UOSCManager::GetAllFloats(Message, ButtonStates);

		// Map button states to global player channels (Unit 1 -> player1..4, Unit 2 -> player5..8, etc.)
		const int32 ChannelsPerUnit = 4;
		int32 BaseChannel = (UnitId - 1) * ChannelsPerUnit;

		for (int32 i = 0; i < ButtonStates.Num(); ++i)
		{
			FName ChannelName = *FString::Printf(TEXT("player%d"), BaseChannel + i + 1);
			SetInputValue(ChannelName, ButtonStates[i]);
		}

		// Send ACK back to the microcontroller on port 8888
		SendACK(IPAddress, SequenceId, TargetClientPort);
		return;
	}

	// 2. Fallback / Legacy individual channel handler (e.g. /player1)
	FString Address = FullPath;
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
