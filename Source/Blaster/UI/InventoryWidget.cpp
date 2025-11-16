// Fill out your copyright notice in the Description page of Project Settings.

#include "InventoryWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Blaster/Components/PlayerInventoryComponent.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Engine/Texture2D.h"

UInventoryWidget::UInventoryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UInventoryWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

void UInventoryWidget::UpdateInventory(const TArray<AParcelActor*>& Items)
{
	if (!InventoryScrollBox)
	{
		return;
	}

	// Clear existing slots
	InventoryScrollBox->ClearChildren();
	ItemSlots.Empty();

	// Add items
	for (AParcelActor* Item : Items)
	{
		if (Item)
		{
			AddItemToDisplay(Item);
		}
	}
}

void UInventoryWidget::AddItemToDisplay(AParcelActor* Item)
{
	if (!Item || !InventoryScrollBox)
	{
		return;
	}

	// Create slot widget
	UInventoryItemSlotWidget* SlotWidget = CreateItemSlot(Item);
	if (SlotWidget)
	{
		InventoryScrollBox->AddChild(SlotWidget);
		ItemSlots.Add(Item, SlotWidget);
	}
}

void UInventoryWidget::RemoveItemFromDisplay(AParcelActor* Item)
{
	if (!Item)
	{
		return;
	}

	UInventoryItemSlotWidget** SlotWidgetPtr = ItemSlots.Find(Item);
	if (SlotWidgetPtr && *SlotWidgetPtr && InventoryScrollBox)
	{
		InventoryScrollBox->RemoveChild(*SlotWidgetPtr);
		ItemSlots.Remove(Item);
	}
}

void UInventoryWidget::SetInventoryVisibility(bool bVisible)
{
	SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

UInventoryItemSlotWidget* UInventoryWidget::CreateItemSlot(AParcelActor* Item)
{
	if (!Item || !ItemSlotWidgetClass)
	{
		return nullptr;
	}

	UInventoryItemSlotWidget* SlotWidget = CreateWidget<UInventoryItemSlotWidget>(GetWorld(), ItemSlotWidgetClass);
	if (SlotWidget)
	{
		SlotWidget->SetItem(Item);
	}

	return SlotWidget;
}

// InventoryItemSlotWidget Implementation

UInventoryItemSlotWidget::UInventoryItemSlotWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInventoryItemSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UInventoryItemSlotWidget::SetItem(AParcelActor* InItem)
{
	Item = InItem;

	if (!Item.IsValid())
	{
		return;
	}

	const FItemData& ItemData = Item->GetItemData();

	// Set item name
	if (ItemNameText)
	{
		ItemNameText->SetText(FText::FromString(ItemData.ItemName));
	}

	// Set item type
	if (ItemTypeText)
	{
		FString TypeString = UEnum::GetValueAsString(ItemData.ItemType);
		ItemTypeText->SetText(FText::FromString(TypeString));
	}

	// Set item description
	if (ItemDescriptionText)
	{
		ItemDescriptionText->SetText(ItemData.ItemDescription);
	}
}

void UInventoryItemSlotWidget::OnTryToUse()
{
	if (!Item.IsValid())
	{
		return;
	}

	// Get player character
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	ABlasterCharacter* Character = Cast<ABlasterCharacter>(PC->GetPawn());
	if (!Character)
	{
		return;
	}

	UPlayerInventoryComponent* InventoryComponent = Character->GetPlayerInventoryComponent();
	if (InventoryComponent)
	{
		InventoryComponent->UseItem(Item.Get());
	}
}

