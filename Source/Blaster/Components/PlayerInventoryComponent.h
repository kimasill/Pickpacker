// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Blaster/Parcel/ParcelActor.h"
#include "Blaster/DataAssets/DA_ItemData.h"
#include "PlayerInventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemCollected, class AParcelActor*, Item, int32, NewCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemRemoved, class AParcelActor*, Item, int32, NewCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemUsed, class AParcelActor*, Item, EItemType, ItemType);

/**
 * Player Inventory Component - Manages player's collected items (unpackaged parcels)
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BLASTER_API UPlayerInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerInventoryComponent();
	
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Collect an item (unpackaged parcel)
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool CollectItem(AParcelActor* Item);

	/**
	 * Remove an item from inventory
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool RemoveItem(AParcelActor* Item);

	/**
	 * Use an item (if usable)
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool UseItem(AParcelActor* Item);

	/**
	 * Check if player has a specific item type
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	bool HasItemType(EItemType ItemType) const;

	/**
	 * Get all items of a specific type
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	TArray<AParcelActor*> GetItemsByType(EItemType ItemType) const;

	/**
	 * Get all usable items
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	TArray<AParcelActor*> GetUsableItems() const;

	/**
	 * Get all collected items
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	TArray<AParcelActor*> GetCollectedItems() const { return CollectedItems; }

	/**
	 * Get inventory count
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	int32 GetInventoryCount() const { return CollectedItems.Num(); }

	/**
	 * Get maximum inventory size
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	int32 GetMaxInventorySize() const { return MaxInventorySize; }

	/**
	 * Set maximum inventory size
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetMaxInventorySize(int32 NewMaxSize) { MaxInventorySize = NewMaxSize; }

	/**
	 * Check if inventory is full
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	bool IsInventoryFull() const { return CollectedItems.Num() >= MaxInventorySize; }

public:
	/** Maximum inventory size */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Inventory Settings")
	int32 MaxInventorySize = 4;

	/** Events */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
	FOnItemCollected OnItemCollected;

	UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
	FOnItemRemoved OnItemRemoved;

	UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
	FOnItemUsed OnItemUsed;

protected:
	/**
	 * Server RPC for collecting item
	 */
	UFUNCTION(Server, Reliable, Category = "Inventory")
	void Server_CollectItem(AParcelActor* Item);

	/**
	 * Server RPC for removing item
	 */
	UFUNCTION(Server, Reliable, Category = "Inventory")
	void Server_RemoveItem(AParcelActor* Item);

	/**
	 * Server RPC for using item
	 */
	UFUNCTION(Server, Reliable, Category = "Inventory")
	void Server_UseItem(AParcelActor* Item);

	/**
	 * Replication callback for collected items
	 */
	UFUNCTION()
	void OnRep_CollectedItems();

private:
	/** Collected items (unpackaged parcels) */
	UPROPERTY(ReplicatedUsing = OnRep_CollectedItems)
	TArray<TObjectPtr<AParcelActor>> CollectedItems;

	/** Debug settings */
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bEnableDebugLogging = true;
};

