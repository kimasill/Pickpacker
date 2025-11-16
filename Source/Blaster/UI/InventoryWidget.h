// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blaster/Parcel/ParcelActor.h"
#include "Blaster/DataAssets/DA_ItemData.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "InventoryWidget.generated.h"

class UInventoryItemSlotWidget;

/**
 * Inventory Widget - Displays player's collected items
 */
UCLASS()
class BLASTER_API UInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UInventoryWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/**
	 * Update inventory display
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void UpdateInventory(const TArray<AParcelActor*>& Items);

	/**
	 * Add item to inventory display
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void AddItemToDisplay(AParcelActor* Item);

	/**
	 * Remove item from inventory display
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RemoveItemFromDisplay(AParcelActor* Item);

	/**
	 * Show/hide inventory
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetInventoryVisibility(bool bVisible);

public:
	/** Scroll box for inventory items */
	UPROPERTY(meta = (BindWidget))
	UScrollBox* InventoryScrollBox;

	/** Item slot widget class */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TSubclassOf<UInventoryItemSlotWidget> ItemSlotWidgetClass;

protected:
	/**
	 * Create item slot widget
	 */
	UInventoryItemSlotWidget* CreateItemSlot(AParcelActor* Item);

private:
	/** Current item slots */
	UPROPERTY()
	TMap<AParcelActor*, UInventoryItemSlotWidget*> ItemSlots;
};

/**
 * Inventory Item Slot Widget - Individual item slot in inventory
 */
UCLASS()
class BLASTER_API UInventoryItemSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UInventoryItemSlotWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;

	/**
	 * Set item data
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory Slot")
	void SetItem(AParcelActor* Item);

	/**
	 * Get item
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory Slot")
	AParcelActor* GetItem() const { return Item.Get(); }

public:
	/** Item name text */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* ItemNameText;

	/** Item type text */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* ItemTypeText;

	/** Item description text */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* ItemDescriptionText;

protected:
	UFUNCTION()
	void OnTryToUse();

private:
	/** Item reference */
	UPROPERTY()
	TWeakObjectPtr<AParcelActor> Item;
};

