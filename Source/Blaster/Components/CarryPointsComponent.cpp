// Fill out your copyright notice in the Description page of Project Settings.

#include "CarryPointsComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"

UCarryPointsComponent::UCarryPointsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	LeftHandleSocketName = FName(TEXT("LeftHandle"));
	RightHandleSocketName = FName(TEXT("RightHandle"));
	LeftHandleOffset = FVector(-50.0f, 0.0f, 0.0f);
	RightHandleOffset = FVector(50.0f, 0.0f, 0.0f);
	SocketRadius = 30.0f;
	
	// Two-person carry settings
	TwoPersonStabilityBonus = 0.3f;
	TwoPersonMovementBonus = 0.15f;
	bEnableTwoPersonCarry = true;
	
	bEnableDebugLogging = true;
	bDrawDebugSockets = false;
}

void UCarryPointsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCarryPointsComponent, CarrySockets);
}

void UCarryPointsComponent::BeginPlay()
{
	Super::BeginPlay();

	InitializeCarrySockets();
}

void UCarryPointsComponent::InitializeCarrySockets()
{
	if (CarrySockets.Num() > 0)
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Log, TEXT("[CarryPointsComponent] Using %d sockets from BP/instance"), CarrySockets.Num());
		}
		return;
	}

	CarrySockets.Empty();

	// Create left handle socket
	FCarrySocket LeftSocket;
	LeftSocket.SocketName = LeftHandleSocketName;
	LeftSocket.RelativeLocation = LeftHandleOffset;
	LeftSocket.RelativeRotation = FRotator::ZeroRotator;
	LeftSocket.bIsOccupied = false;
	LeftSocket.OccupyingCharacter = nullptr;
	CarrySockets.Add(LeftSocket);

	// Create right handle socket
	FCarrySocket RightSocket;
	RightSocket.SocketName = RightHandleSocketName;
	RightSocket.RelativeLocation = RightHandleOffset;
	RightSocket.RelativeRotation = FRotator::ZeroRotator;
	RightSocket.bIsOccupied = false;
	RightSocket.OccupyingCharacter = nullptr;
	CarrySockets.Add(RightSocket);

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[CarryPointsComponent] Initialized %d carry sockets"), CarrySockets.Num());
	}
}

bool UCarryPointsComponent::TryAttachToSocket(ACharacter* Character, const FName& SocketName)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CarryPointsComponent] Only server can attach to socket"));
		return false;
	}

	if (!Character)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CarryPointsComponent] Invalid character"));
		return false;
	}

	const bool bAny =
		SocketName.IsNone() ||
		SocketName == FName(TEXT("CarrySocket"));

	if (bAny) {
		for (FCarrySocket& S : CarrySockets)
		{
			if(!S.bIsOccupied)
			{
				S.bIsOccupied = true;
				S.OccupyingCharacter = Character;

				if (bEnableDebugLogging)
				{
					UE_LOG(LogTemp, Log, TEXT("[CarryPointsComponent] Character %s attached to auto-picked socket %s"),
						*Character->GetName(), *S.SocketName.ToString());
				}

				OnSocketOccupied.Broadcast(S.SocketName, Character);
				return true;
			}
		}
	}

	int32 SocketIndex = FindSocketIndex(SocketName);
	if (SocketIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CarryPointsComponent] Socket not found: %s"), *SocketName.ToString());
		return false;
	}

	FCarrySocket& Socket = CarrySockets[SocketIndex];
	if (Socket.bIsOccupied)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CarryPointsComponent] Socket already occupied: %s"), *SocketName.ToString());
		return false;
	}

	// Attach character to socket
	Socket.bIsOccupied = true;
	Socket.OccupyingCharacter = Character;

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[CarryPointsComponent] Character %s attached to socket %s"),
			*Character->GetName(), *SocketName.ToString());
	}

	OnSocketOccupied.Broadcast(SocketName, Character);
	return true;
}

bool UCarryPointsComponent::DetachFromSocket(ACharacter* Character, const FName& SocketName)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CarryPointsComponent] Only server can detach from socket"));
		return false;
	}

	int32 SocketIndex = FindSocketIndex(SocketName);
	if (SocketIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CarryPointsComponent] Socket not found: %s"), *SocketName.ToString());
		return false;
	}

	FCarrySocket& Socket = CarrySockets[SocketIndex];
	if (!Socket.bIsOccupied || Socket.OccupyingCharacter != Character)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CarryPointsComponent] Character not attached to socket: %s"), *SocketName.ToString());
		return false;
	}

	// Detach character from socket
	Socket.bIsOccupied = false;
	Socket.OccupyingCharacter = nullptr;

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[CarryPointsComponent] Character %s detached from socket %s"),
			*Character->GetName(), *SocketName.ToString());
	}

	OnSocketFreed.Broadcast(SocketName, Character);
	return true;
}

void UCarryPointsComponent::DetachAllCharacters()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CarryPointsComponent] Only server can detach all characters"));
		return;
	}

	for (FCarrySocket& Socket : CarrySockets)
	{
		if (Socket.bIsOccupied && Socket.OccupyingCharacter)
		{
			ACharacter* Character = Socket.OccupyingCharacter;
			Socket.bIsOccupied = false;
			Socket.OccupyingCharacter = nullptr;

			if (bEnableDebugLogging)
			{
				UE_LOG(LogTemp, Log, TEXT("[CarryPointsComponent] Character %s detached from socket %s"),
					*Character->GetName(), *Socket.SocketName.ToString());
			}

			OnSocketFreed.Broadcast(Socket.SocketName, Character);
		}
	}
}

FVector UCarryPointsComponent::GetSocketWorldLocation(const FName& SocketName) const
{
	int32 SocketIndex = FindSocketIndex(SocketName);
	if (SocketIndex == INDEX_NONE)
	{
		return GetComponentLocation();
	}

	const FCarrySocket& Socket = CarrySockets[SocketIndex];
	return GetComponentTransform().TransformPosition(Socket.RelativeLocation);
}

FRotator UCarryPointsComponent::GetSocketWorldRotation(const FName& SocketName) const
{
	int32 SocketIndex = FindSocketIndex(SocketName);
	if (SocketIndex == INDEX_NONE)
	{
		return GetComponentRotation();
	}

	const FCarrySocket& Socket = CarrySockets[SocketIndex];
	FQuat RelativeQuat = FQuat(Socket.RelativeRotation);
	FQuat WorldQuat = GetComponentTransform().TransformRotation(RelativeQuat);
	return WorldQuat.Rotator();
}

bool UCarryPointsComponent::IsSocketAvailable(const FName& SocketName) const
{
	int32 SocketIndex = FindSocketIndex(SocketName);
	if (SocketIndex == INDEX_NONE)
	{
		return false;
	}

	return !CarrySockets[SocketIndex].bIsOccupied;
}

int32 UCarryPointsComponent::GetAvailableSocketCount() const
{
	int32 Count = 0;
	for (const FCarrySocket& Socket : CarrySockets)
	{
		if (!Socket.bIsOccupied)
		{
			Count++;
		}
	}
	return Count;
}

int32 UCarryPointsComponent::GetOccupiedSocketCount() const
{
	int32 Count = 0;
	for (const FCarrySocket& Socket : CarrySockets)
	{
		if (Socket.bIsOccupied)
		{
			Count++;
		}
	}
	return Count;
}

FCarrySocket UCarryPointsComponent::GetCarrySocket(const FName& SocketName) const
{
	int32 SocketIndex = FindSocketIndex(SocketName);
	if (SocketIndex == INDEX_NONE)
	{
		return FCarrySocket(); // Return empty socket
	}

	return CarrySockets[SocketIndex];
}

int32 UCarryPointsComponent::FindSocketIndex(const FName& SocketName) const
{
	for (int32 i = 0; i < CarrySockets.Num(); ++i)
	{
		if (CarrySockets[i].SocketName == SocketName)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

bool UCarryPointsComponent::IsTwoPersonCarry() const
{
	if (!bEnableTwoPersonCarry)
	{
		return false;
	}

	int32 OccupiedCount = GetOccupiedSocketCount();
	return OccupiedCount >= 2;
}

float UCarryPointsComponent::GetStabilityBonus() const
{
	if (IsTwoPersonCarry())
	{
		return TwoPersonStabilityBonus;
	}
	return 0.0f;
}

float UCarryPointsComponent::GetMovementSpeedBonus() const
{
	if (IsTwoPersonCarry())
	{
		return TwoPersonMovementBonus;
	}
	return 0.0f;
}

void UCarryPointsComponent::HandleSocketOccupied(const FName& SocketName, ACharacter* Character)
{
	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[CarryPointsComponent] Socket occupied - Socket: %s, Character: %s"),
			*SocketName.ToString(), Character ? *Character->GetName() : TEXT("None"));
	}
}

void UCarryPointsComponent::HandleSocketFreed(const FName& SocketName, ACharacter* Character)
{
	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[CarryPointsComponent] Socket freed - Socket: %s, Character: %s"),
			*SocketName.ToString(), Character ? *Character->GetName() : TEXT("None"));
	}
}

void UCarryPointsComponent::UpdateSocketTransforms()
{
	// This would be called if socket positions need to be updated dynamically
	// For now, sockets are static relative to the component
}
