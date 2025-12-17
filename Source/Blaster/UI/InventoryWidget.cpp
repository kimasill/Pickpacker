// Fill out your copyright notice in the Description page of Project Settings.

#include "InventoryWidget.h"
#include "Components/ScrollBox.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Blaster/Components/PlayerInventoryComponent.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Blaster/DataAssets/DA_ItemData.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Texture2D.h"
#include "UObject/UnrealType.h"

UInventoryWidget::UInventoryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Find and bind to PlayerInventoryComponent's OnInventoryUpdated event
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (ABlasterCharacter* Character = Cast<ABlasterCharacter>(PC->GetPawn()))
		{
			PlayerInventoryComponent = Character->GetPlayerInventoryComponent();
			if (PlayerInventoryComponent)
			{
				PlayerInventoryComponent->OnInventoryUpdated.AddDynamic(this, &UInventoryWidget::HandleInventoryUpdated);
				
				// Update with current inventory immediately
				HandleInventoryUpdated(PlayerInventoryComponent->GetCollectedItems(), PlayerInventoryComponent->GetInventoryCount());
			}
		}
	}
}

void UInventoryWidget::NativeDestruct()
{
	// Unbind from PlayerInventoryComponent event
	if (PlayerInventoryComponent)
	{
		PlayerInventoryComponent->OnInventoryUpdated.RemoveDynamic(this, &UInventoryWidget::HandleInventoryUpdated);
		PlayerInventoryComponent = nullptr;
	}

	Super::NativeDestruct();
}

void UInventoryWidget::UpdateInventory(const TArray<AParcelActor*>& Items)
{
	if (!InventoryVerticalBox)
	{
		UE_LOG(LogTemp, Warning, TEXT("[InventoryWidget] UpdateInventory: InventoryVerticalBox is null! Make sure it's added to widget designer with BindWidget."));
		return;
	}

	// Clear existing slots
	InventoryVerticalBox->ClearChildren();
	ItemSlots.Empty();

	// Add items up to MaxDisplaySlots
	int32 DisplayCount = FMath::Min(Items.Num(), MaxDisplaySlots);
	for (int32 i = 0; i < DisplayCount; ++i)
	{
		if (Items[i])
		{
			AddItemToDisplay(Items[i], i);
		}
	}

	// Fill remaining slots with empty slots if needed
	for (int32 i = DisplayCount; i < MaxDisplaySlots; ++i)
	{
		// Create empty slot widget (optional - can be handled in Blueprint)
		// For now, we'll just leave them empty
	}
}

void UInventoryWidget::AddItemToDisplay(AParcelActor* Item, int32 SlotIndex)
{
	if (!Item || !InventoryVerticalBox)
	{
		return;
	}

	// Create slot widget
	UInventoryItemSlotWidget* SlotWidget = CreateItemSlot(Item, SlotIndex);
	if (SlotWidget)
	{
		InventoryVerticalBox->AddChild(SlotWidget);
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
	if (SlotWidgetPtr && *SlotWidgetPtr && InventoryVerticalBox)
	{
		InventoryVerticalBox->RemoveChild(*SlotWidgetPtr);
		ItemSlots.Remove(Item);
	}
}

void UInventoryWidget::SetInventoryVisibility(bool bVisible)
{
	SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

UInventoryItemSlotWidget* UInventoryWidget::GetSlotWidgetAt(int32 Index) const
{
	if (InventoryVerticalBox && InventoryVerticalBox->GetChildrenCount() > Index)
	{
		return Cast<UInventoryItemSlotWidget>(InventoryVerticalBox->GetChildAt(Index));
	}
	return nullptr;
}

UInventoryItemSlotWidget* UInventoryWidget::CreateItemSlot(AParcelActor* Item, int32 SlotIndex)
{
	if (!Item || !ItemSlotWidgetClass)
	{
		return nullptr;
	}

	UInventoryItemSlotWidget* SlotWidget = CreateWidget<UInventoryItemSlotWidget>(GetWorld(), ItemSlotWidgetClass);
	if (SlotWidget)
	{
		SlotWidget->SetItem(Item, SlotIndex);
	}

	return SlotWidget;
}

void UInventoryWidget::HandleInventoryUpdated(const TArray<AParcelActor*>& Items, int32 Count)
{
	if (!InventoryVerticalBox)
	{
		UE_LOG(LogTemp, Warning, TEXT("[InventoryWidget] HandleInventoryUpdated: InventoryVerticalBox is null! Widget may not be properly initialized."));
		return;
	}
	
	UE_LOG(LogTemp, Log, TEXT("[InventoryWidget] HandleInventoryUpdated called with %d items"), Items.Num());
	UpdateInventory(Items);
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

void UInventoryItemSlotWidget::SetItem(AParcelActor* InItem, int32 SlotIndex)
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
		// Remove enum prefix if present (e.g., "EItemType::" -> "")
		TypeString.RemoveFromStart(TEXT("EItemType::"));
		ItemTypeText->SetText(FText::FromString(TypeString));
	}

	// Set key text (1-9)
	if (KeyText)
	{
		// SlotIndex is 0-based, display as 1-9
		int32 KeyNumber = SlotIndex + 1;
		if (KeyNumber >= 1 && KeyNumber <= 9)
		{
			KeyText->SetText(FText::FromString(FString::Printf(TEXT("%d"), KeyNumber)));
		}
		else
		{
			KeyText->SetText(FText::GetEmpty());
		}
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

