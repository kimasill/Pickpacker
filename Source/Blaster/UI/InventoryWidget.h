// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blaster/Parcel/ParcelActor.h"
#include "Blaster/DataAssets/DA_ItemData.h"
#include "Components/ScrollBox.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
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
	void AddItemToDisplay(AParcelActor* Item, int32 SlotIndex);

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

	/**
	 * Get slot widget at index
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	UInventoryItemSlotWidget* GetSlotWidgetAt(int32 Index) const;

protected:
	/**
	 * Handle inventory updated event from PlayerInventoryComponent
	 */
	UFUNCTION()
	void HandleInventoryUpdated(const TArray<AParcelActor*>& Items, int32 Count);

private:
	/** PlayerInventoryComponent reference for event binding */
	UPROPERTY()
	class UPlayerInventoryComponent* PlayerInventoryComponent;

public:
	/** Vertical box for inventory items (vertical list UI) */
	UPROPERTY(meta = (BindWidget))
	UVerticalBox* InventoryVerticalBox;

	/** Item slot widget class */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TSubclassOf<UInventoryItemSlotWidget> ItemSlotWidgetClass;

	/** Maximum number of slots to display */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	int32 MaxDisplaySlots = 9;

protected:
	/**
	 * Create item slot widget
	 */
	UInventoryItemSlotWidget* CreateItemSlot(AParcelActor* Item, int32 SlotIndex);

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
	void SetItem(AParcelActor* Item, int32 SlotIndex);

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

	UPROPERTY(meta = (BindWidget))
	UImage* KeyIcon;

protected:
	UFUNCTION()
	void OnTryToUse();

private:
	/** Item reference */
	UPROPERTY()
	TWeakObjectPtr<AParcelActor> Item;

	/** Slot index */
	FName SlotActionName;
};

