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
#include "Blaster/UI/ParcelTagDisplayUtils.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Texture2D.h"
#include "UObject/UnrealType.h"
#include "InputLibrary.h"
#include "TimerManager.h"
#include "Engine/World.h"

namespace
{
    static const TArray<FName> SlotActionNames = {
        TEXT("1"), TEXT("2"), TEXT("3"),
        TEXT("4"), TEXT("5"), TEXT("6"),
        TEXT("7"), TEXT("8"), TEXT("9")
    };
}

UInventoryWidget::UInventoryWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UInventoryWidget::NativeConstruct()
{
    Super::NativeConstruct();

    TryBindToInventoryComponent();
}

void UInventoryWidget::TryBindToInventoryComponent()
{
    if (PlayerInventoryComponent)
    {
        return;
    }

    APlayerController* PC = GetOwningPlayer();
    if (!PC)
    {
        return;
    }

    ABlasterCharacter* Character = Cast<ABlasterCharacter>(PC->GetPawn());
    if (!Character)
    {
        // 패키징 빌드: Pawn이 아직 없을 수 있음. 짧은 지연 후 재시도
        if (RetryBindCount < MaxRetryBindCount && GetWorld())
        {
            ++RetryBindCount;
            GetWorld()->GetTimerManager().SetTimer(RetryBindTimerHandle, this, &UInventoryWidget::TryBindToInventoryComponent, 0.1f, false);
        }
        return;
    }

    PlayerInventoryComponent = Character->GetPlayerInventoryComponent();
    if (PlayerInventoryComponent)
    {
        PlayerInventoryComponent->OnInventoryUpdated.AddDynamic(this, &UInventoryWidget::HandleInventoryUpdated);
        HandleInventoryUpdated(PlayerInventoryComponent->GetCollectedItems(), PlayerInventoryComponent->GetInventoryCount());
    }
}

void UInventoryWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RetryBindTimerHandle);
	}
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

	// Add items up to MaxDisplaySlots (파괴된 파슬 제외)
	int32 DisplayCount = FMath::Min(Items.Num(), MaxDisplaySlots);
	for (int32 i = 0; i < DisplayCount; ++i)
	{
		if (Items[i] && IsValid(Items[i]))
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
	if (!Item || !IsValid(Item) || !InventoryVerticalBox)
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
	if (!Item || !IsValid(Item))
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
	if (!Item)
	{
		return nullptr;
	}
	if (!ItemSlotWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[InventoryWidget] ItemSlotWidgetClass is null. Set it in Blueprint (e.g. WBP_InventoryWidget)."));
		return nullptr;
	}

	APlayerController* PC = GetOwningPlayer();
	UInventoryItemSlotWidget* SlotWidget = PC
		? CreateWidget<UInventoryItemSlotWidget>(PC, ItemSlotWidgetClass)
		: CreateWidget<UInventoryItemSlotWidget>(GetWorld(), ItemSlotWidgetClass);
	if (SlotWidget)
	{
		SlotWidget->SetParcelTagDisplayTable(ParcelTagDisplayTable);
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
	// Bind use action
}

void UInventoryItemSlotWidget::SetItem(AParcelActor* InItem, int32 SlotIndex)
{
	Item = InItem;

	if (!Item.IsValid())
	{
		if (KeyIcon)
		{
			KeyIcon->SetBrushFromTexture(nullptr);
		}
		return;
	}

	const FString ParcelName = Item->GetParcelDisplayName();
	const FItemData& ItemData = Item->GetItemData();

	// Set item name
	if (ItemNameText)
	{
		if (!ParcelName.IsEmpty())
		{
			ItemNameText->SetText(FText::FromString(ParcelName));
		}
		else
		{
			ItemNameText->SetText(FText::FromString(ItemData.ItemName));
		}
	}

	// Set item type
	if (ItemTypeText)
	{
		FString TypeString;
		const FGameplayTag ParcelTag = Item->GetParcelTag();
		if (ParcelTag.IsValid())
		{
			FText DisplayText;
			if (FParcelTagDisplayUtils::TryGetTagDisplayText(ParcelTagDisplayTable, ParcelTag, DisplayText))
			{
				TypeString = DisplayText.ToString();
			}
			else
			{
				TypeString = FParcelTagDisplayUtils::GetTagLastPart(ParcelTag);
			}
		}
		else if (ItemData.ItemType != EItemType::Unknown)
		{
			TypeString = UEnum::GetValueAsString(ItemData.ItemType);
			TypeString.RemoveFromStart(TEXT("EItemType::"));
		}
		else
		{
			TypeString = TEXT("Unknown");
		}
		ItemTypeText->SetText(FText::FromString(TypeString));
	}

	// Set key icon (1-9)
	if (KeyIcon)
	{
		const int32 KeyNumber = SlotIndex + 1;
		if (KeyNumber >= 1 && KeyNumber <= 9)
		{
			const int32 IconIndex = KeyNumber - 1;
			if (SlotActionNames.IsValidIndex(IconIndex))
			{
				const FKey ActionKey(SlotActionNames[IconIndex]);
				if (UTexture2D* IconTexture = UInputLibrary::GetIconForKey(ActionKey))
				{
					KeyIcon->SetBrushFromTexture(IconTexture);
				}
				else
				{
					KeyIcon->SetBrushFromTexture(nullptr);
				}
			}
			else
			{
				KeyIcon->SetBrushFromTexture(nullptr);
			}
		}
		else
		{
			KeyIcon->SetBrushFromTexture(nullptr);
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

