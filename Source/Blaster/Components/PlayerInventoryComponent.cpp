// Fill out your copyright notice in the Description page of Project Settings.

#include "PlayerInventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Blaster/Components/InteractionComponent.h" // added to resolve incomplete type
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"

UPlayerInventoryComponent::UPlayerInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	MaxInventorySize = 4;
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
	
	// Hide the item in world and disable physics
	Item->SetActorHiddenInGame(true);
	if (UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(Item->GetRootComponent()))
	{
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComp->SetSimulatePhysics(false);
	}

	OnItemCollected.Broadcast(Item, CollectedItems.Num());
	
	// Update UI
	UpdateInventoryUI();
	
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
		OnItemRemoved.Broadcast(Item, CollectedItems.Num());

		// Update UI
		UpdateInventoryUI();

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

AParcelActor* UPlayerInventoryComponent::EquipItemFromInventory(int32 SlotIndex)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		Server_EquipItemFromInventory(SlotIndex);
		return nullptr;
	}

	// Server-side equip
	if (!CollectedItems.IsValidIndex(SlotIndex))
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PlayerInventoryComponent] Invalid slot index: %d"), SlotIndex);
		}
		return nullptr;
	}

	AParcelActor* Item = CollectedItems[SlotIndex];
	if (!Item)
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PlayerInventoryComponent] Item at slot %d is null"), SlotIndex);
		}
		return nullptr;
	}

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter)
	{
		return nullptr;
	}

	// Check if character is already carrying something
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(OwnerCharacter);
	if (BlasterCharacter)
	{
		UInteractionComponent* InteractionComp = BlasterCharacter->GetInteractionComponent();
		if (InteractionComp && InteractionComp->GetCarriedParcel())
		{
			if (bEnableDebugLogging)
			{
				UE_LOG(LogTemp, Warning, TEXT("[PlayerInventoryComponent] Cannot equip - Character is already carrying a parcel"));
			}
			return nullptr;
		}
	}

	// Remove from inventory
	if (CollectedItems.Remove(Item) > 0)
	{
		// Show the item in world (collision/physics handled by RequestAttach)
		Item->SetActorHiddenInGame(false);

		// Spawn item at character location
		FVector SpawnLocation = OwnerCharacter->GetActorLocation() + OwnerCharacter->GetActorForwardVector() * 100.0f;
		Item->SetActorLocation(SpawnLocation);

		// Attach to character (this will disable physics appropriately)
		Item->RequestAttach(OwnerCharacter, FName("CarrySocket"));

		OnItemRemoved.Broadcast(Item, CollectedItems.Num());

		// Update UI
		UpdateInventoryUI();

		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Log, TEXT("[PlayerInventoryComponent] Item equipped from inventory - %s (Slot: %d)"),
				*Item->GetItemData().ItemName, SlotIndex);
		}

		return Item;
	}

	return nullptr;
}

AParcelActor* UPlayerInventoryComponent::GetItemAtSlot(int32 SlotIndex) const
{
	if (CollectedItems.IsValidIndex(SlotIndex))
	{
		return CollectedItems[SlotIndex];
	}
	return nullptr;
}

bool UPlayerInventoryComponent::PutCarriedParcelIntoInventory(int32 SlotIndex)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		Server_PutCarriedParcelIntoInventory(SlotIndex);
		return true;
	}

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter) { return false; }

	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(OwnerCharacter);
	if (!BlasterCharacter) { return false; }

	UInteractionComponent* InteractionComp = BlasterCharacter->GetInteractionComponent();
	if (!InteractionComp) { return false; }

	AParcelActor* CarriedParcel = InteractionComp->GetCarriedParcel();
	if (!CarriedParcel) { return false; }

	if (!CarriedParcel->IsItem())
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PlayerInventoryComponent] Cannot put non-item parcel into inventory"));
		}
		return false;
	}

	// 1) 대상 슬롯 결정
	int32 TargetSlot = SlotIndex;
	if (TargetSlot < 0 || TargetSlot >= MaxInventorySize)
	{
		for (int32 i = 0; i < MaxInventorySize; ++i)
		{
			if (!CollectedItems.IsValidIndex(i) || CollectedItems[i] == nullptr)
			{
				TargetSlot = i;
				break;
			}
		}
	}

	// 2) 슬롯 가용성 검사
	if (TargetSlot < 0 || TargetSlot >= MaxInventorySize)
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PlayerInventoryComponent] No available slot for inventory"));
		}
		return false;
	}
	if (CollectedItems.IsValidIndex(TargetSlot) && CollectedItems[TargetSlot] != nullptr)
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PlayerInventoryComponent] Slot %d is already occupied"), TargetSlot);
		}
		return false;
	}

	// 3) 배열 크기 보장
	while (CollectedItems.Num() <= TargetSlot)
	{
		CollectedItems.Add(nullptr);
	}

	// 4) 먼저 '드랍'으로 부착 해제 (물리/임펄스는 0)
	CarriedParcel->RequestDrop(FVector::ZeroVector);

	// 5) 인벤토리에 배치
	CollectedItems[TargetSlot] = CarriedParcel;

	// 6) 가시성/충돌/물리 비활성화는 '드랍' 이후에 적용
	CarriedParcel->SetActorHiddenInGame(true);
	if (UStaticMeshComponent* MeshComp = CarriedParcel->GetParcelMesh())
	{
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComp->SetSimulatePhysics(false);
	}

	// 7) 캐릭터의 carried 상태 정리(서버 권한 OK)
	InteractionComp->SetCarriedParcel(nullptr);

	OnItemCollected.Broadcast(CarriedParcel, CollectedItems.Num());

	// UI 갱신
	UpdateInventoryUI();

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PlayerInventoryComponent] Put carried parcel into inventory - %s (Slot: %d)"),
			*CarriedParcel->GetItemData().ItemName, TargetSlot);
	}

	return true;
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

void UPlayerInventoryComponent::Server_EquipItemFromInventory_Implementation(int32 SlotIndex)
{
	EquipItemFromInventory(SlotIndex);
}

void UPlayerInventoryComponent::Server_PutCarriedParcelIntoInventory_Implementation(int32 SlotIndex)
{
	PutCarriedParcelIntoInventory(SlotIndex);
}

void UPlayerInventoryComponent::OnRep_CollectedItems()
{
	// Replication callback - can be used for UI updates
	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PlayerInventoryComponent] Collected items replicated - Count: %d"), CollectedItems.Num());
	}

	// Update inventory UI
	UpdateInventoryUI();
	
	// Movement speed will be updated in next tick by BlasterCharacter::UpdateMovementSpeedFromCarriedParcel()
}

void UPlayerInventoryComponent::UpdateInventoryUI()
{
	// Broadcast inventory update event for UI widgets to listen to
	OnInventoryUpdated.Broadcast(CollectedItems, CollectedItems.Num());
}

bool UPlayerInventoryComponent::CollectItemAndAttach(AParcelActor* Item, bool bAttachToOwner)
{
    if (!Item)
    {
        return false;
    }

    // Collect first
    if (!CollectItem(Item))
    {
        return false;
    }

    if (!bAttachToOwner)
    {
        return true;
    }

    // Attach to owning character if requested
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (!OwnerCharacter)
    {
        return true; // collected ok
    }

    // Unhide to attach visually; keep collision disabled
    Item->SetActorHiddenInGame(false);
    if (UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(Item->GetRootComponent()))
    {
        MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        MeshComp->SetSimulatePhysics(false);
    }
    Item->RequestAttach(OwnerCharacter, FName("CarrySocket"));
    return true;
}

