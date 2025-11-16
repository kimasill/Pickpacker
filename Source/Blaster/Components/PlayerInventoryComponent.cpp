// Fill out your copyright notice in the Description page of Project Settings.

#include "PlayerInventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"
#include "GameFramework/Actor.h"
#include "Blaster/Character/BlasterCharacter.h"

UPlayerInventoryComponent::UPlayerInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	MaxInventorySize = 10;
	bEnableDebugLogging = true;
	SetIsReplicatedByDefault(true);
}

void UPlayerInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UPlayerInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UPlayerInventoryComponent, CollectedItems);
	DOREPLIFETIME(UPlayerInventoryComponent, MaxInventorySize);
}

bool UPlayerInventoryComponent::CollectItem(AParcelActor* Item)
{
	if (!Item)
	{
		return false;
	}

	if (!Item->IsItem())
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PlayerInventoryComponent] Cannot collect - Item is not an item (packaged)"));
		}
		return false;
	}

	if (IsInventoryFull())
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PlayerInventoryComponent] Cannot collect - Inventory is full"));
		}
		return false;
	}

	if (CollectedItems.Contains(Item))
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PlayerInventoryComponent] Cannot collect - Item already in inventory"));
		}
		return false;
	}

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		Server_CollectItem(Item);
		return true;
	}

	// Server-side collection
	CollectedItems.Add(Item);
	
	// Hide the item in world
	Item->SetActorHiddenInGame(true);
	Item->SetActorEnableCollision(false);

	OnItemCollected.Broadcast(Item, CollectedItems.Num());
	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PlayerInventoryComponent] Item collected - %s (Count: %d)"),
			*Item->GetItemData().ItemName, CollectedItems.Num());
	}

	return true;
}

bool UPlayerInventoryComponent::RemoveItem(AParcelActor* Item)
{
	if (!Item)
	{
		return false;
	}

	 if (!GetOwner() || !GetOwner()->HasAuthority())
    {
		Server_RemoveItem(Item);
		return true;
	}

	// Server-side removal
	if (CollectedItems.Remove(Item) > 0)
	{
		// Show the item in world again
		Item->SetActorHiddenInGame(false);
		Item->SetActorEnableCollision(true);

		OnItemRemoved.Broadcast(Item, CollectedItems.Num());

		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Log, TEXT("[PlayerInventoryComponent] Item removed - %s (Count: %d)"),
				*Item->GetItemData().ItemName, CollectedItems.Num());
		}

		return true;
	}

	return false;
}

bool UPlayerInventoryComponent::UseItem(AParcelActor* Item)
{
	if (!Item)
	{
		return false;
	}

	if (!Item->IsUsable())
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PlayerInventoryComponent] Cannot use - Item is not usable"));
		}
		return false;
	}

	if (!CollectedItems.Contains(Item))
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PlayerInventoryComponent] Cannot use - Item not in inventory"));
		}
		return false;
	}

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		Server_UseItem(Item);
		return true;
	}

	// Server-side use
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter)
	{
		return false;
	}

	bool bSuccess = Item->UseItem(OwnerCharacter);
	
	if (bSuccess)
	{
		OnItemUsed.Broadcast(Item, Item->GetItemType());

		// If item is consumed, remove from inventory
		if (Item->GetItemData().bConsumedOnUse)
		{
			RemoveItem(Item);
		}
	}

	return bSuccess;
}

bool UPlayerInventoryComponent::HasItemType(EItemType ItemType) const
{
	for (AParcelActor* Item : CollectedItems)
	{
		if (Item && Item->GetItemType() == ItemType)
		{
			return true;
		}
	}
	return false;
}

TArray<AParcelActor*> UPlayerInventoryComponent::GetItemsByType(EItemType ItemType) const
{
	TArray<AParcelActor*> Result;
	for (AParcelActor* Item : CollectedItems)
	{
		if (Item && Item->GetItemType() == ItemType)
		{
			Result.Add(Item);
		}
	}
	return Result;
}

TArray<AParcelActor*> UPlayerInventoryComponent::GetUsableItems() const
{
	TArray<AParcelActor*> Result;
	for (AParcelActor* Item : CollectedItems)
	{
		if (Item && Item->IsUsable())
		{
			Result.Add(Item);
		}
	}
	return Result;
}

void UPlayerInventoryComponent::Server_CollectItem_Implementation(AParcelActor* Item)
{
	CollectItem(Item);
}

void UPlayerInventoryComponent::Server_RemoveItem_Implementation(AParcelActor* Item)
{
	RemoveItem(Item);
}

void UPlayerInventoryComponent::Server_UseItem_Implementation(AParcelActor* Item)
{
	UseItem(Item);
}

void UPlayerInventoryComponent::OnRep_CollectedItems()
{
	// Replication callback - can be used for UI updates
	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PlayerInventoryComponent] Collected items replicated - Count: %d"), CollectedItems.Num());
	}
}

