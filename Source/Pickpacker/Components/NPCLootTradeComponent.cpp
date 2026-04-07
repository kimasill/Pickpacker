// NPCLootTradeComponent.cpp

#include "NPCLootTradeComponent.h"
#include "GameFramework/Character.h"
#include "Components/EscapeProgressComponent.h"
#include "GameState/PickpackerGameState.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

UNPCLootTradeComponent::UNPCLootTradeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UNPCLootTradeComponent::BeginPlay()
{
	Super::BeginPlay();
}

// =========================================================================
// API
// =========================================================================

bool UNPCLootTradeComponent::OpenTrade(ACharacter* Interactor)
{
	if (bTradeOpen || !Interactor)
	{
		return false;
	}

	bTradeOpen = true;
	OnTradeOpened.Broadcast(Interactor);
	return true;
}

void UNPCLootTradeComponent::CloseTrade()
{
	if (!bTradeOpen)
	{
		return;
	}

	bTradeOpen = false;
	OnTradeClosed.Broadcast();
}

TArray<FLootTradeEntry> UNPCLootTradeComponent::GetAvailableEntries() const
{
	TArray<FLootTradeEntry> Result;

	for (const FLootTradeEntry& Entry : TradeInventory)
	{
		if (Entry.Quantity <= 0)
		{
			continue;
		}

		if (Entry.RequiredWorldFlag.IsValid() && !IsWorldFlagSet(Entry.RequiredWorldFlag))
		{
			continue;
		}

		Result.Add(Entry);
	}

	return Result;
}

bool UNPCLootTradeComponent::TryPurchase(ACharacter* Buyer, const FGameplayTag& ItemTag, int32 Quantity)
{
	if (!Buyer || !ItemTag.IsValid() || Quantity <= 0)
	{
		return false;
	}

	// Find entry
	FLootTradeEntry* Entry = nullptr;
	for (FLootTradeEntry& E : TradeInventory)
	{
		if (E.ItemTag == ItemTag && E.Quantity >= Quantity)
		{
			if (E.RequiredWorldFlag.IsValid() && !IsWorldFlagSet(E.RequiredWorldFlag))
			{
				continue;
			}
			Entry = &E;
			break;
		}
	}

	if (!Entry)
	{
		return false;
	}

	// Check credits if needed
	if (Entry->CreditCost > 0)
	{
		APickpackerGameState* GS = Cast<APickpackerGameState>(UGameplayStatics::GetGameState(this));
		if (!GS || GS->GetTeamCredits() < Entry->CreditCost * Quantity)
		{
			return false;
		}
		GS->ApplyCreditDelta(-(Entry->CreditCost * Quantity), FString::Printf(TEXT("NPC Trade: %s x%d"), *ItemTag.ToString(), Quantity));
	}

	Entry->Quantity -= Quantity;

	OnTradeCompleted.Broadcast(Buyer, ItemTag, Quantity);
	return true;
}

bool UNPCLootTradeComponent::RemoveFromInventory(const FGameplayTag& ItemTag, int32 Quantity)
{
	for (FLootTradeEntry& Entry : TradeInventory)
	{
		if (Entry.ItemTag == ItemTag)
		{
			if (Entry.Quantity >= Quantity)
			{
				Entry.Quantity -= Quantity;
				return true;
			}
			return false;
		}
	}
	return false;
}

void UNPCLootTradeComponent::AddToInventory(const FLootTradeEntry& NewEntry)
{
	// Merge with existing entry if same tag
	for (FLootTradeEntry& Entry : TradeInventory)
	{
		if (Entry.ItemTag == NewEntry.ItemTag)
		{
			Entry.Quantity += NewEntry.Quantity;
			return;
		}
	}
	TradeInventory.Add(NewEntry);
}

bool UNPCLootTradeComponent::HasItem(const FGameplayTag& ItemTag) const
{
	return GetItemQuantity(ItemTag) > 0;
}

int32 UNPCLootTradeComponent::GetItemQuantity(const FGameplayTag& ItemTag) const
{
	for (const FLootTradeEntry& Entry : TradeInventory)
	{
		if (Entry.ItemTag == ItemTag)
		{
			return Entry.Quantity;
		}
	}
	return 0;
}

// =========================================================================
// Internal
// =========================================================================

bool UNPCLootTradeComponent::IsWorldFlagSet(const FGameplayTag& Flag) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	APickpackerGameState* GS = Cast<APickpackerGameState>(World->GetGameState());
	if (!GS)
	{
		return false;
	}

	UEscapeProgressComponent* EscapeProgress = GS->GetEscapeProgressComponent();
	if (!EscapeProgress)
	{
		return false;
	}

	return EscapeProgress->GetWorldFlag(Flag) != 0;
}
