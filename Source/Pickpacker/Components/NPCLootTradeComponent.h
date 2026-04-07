// NPCLootTradeComponent - Modular loot/trade module for NPC actors

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PickpackerTypes/CoreLoopTypes.h"
#include "NPCLootTradeComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnTradeCompleted, class ACharacter*, Buyer, FGameplayTag, ItemTag, int32, Quantity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTradeOpened, class ACharacter*, Interactor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTradeClosed);

/**
 * Modular loot / trade component for NPC actors.
 * Holds a tag-based inventory that players can buy from or sell to.
 * Entries can be conditioned on world flags.
 */
UCLASS(ClassGroup = (NPC), meta = (BlueprintSpawnableComponent))
class PICKPACKER_API UNPCLootTradeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPCLootTradeComponent();

	// --- Configuration ---------------------------------------------------

	/** Trade inventory entries */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade")
	TArray<FLootTradeEntry> TradeInventory;

	// --- Runtime State ---------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "Trade")
	bool bTradeOpen = false;

	// --- API -------------------------------------------------------------

	/** Open trade UI with a player */
	UFUNCTION(BlueprintCallable, Category = "Trade")
	bool OpenTrade(class ACharacter* Interactor);

	/** Close trade */
	UFUNCTION(BlueprintCallable, Category = "Trade")
	void CloseTrade();

	/** Get entries available to the player (filtered by world flags) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Trade")
	TArray<FLootTradeEntry> GetAvailableEntries() const;

	/** Attempt to purchase an item by tag. Returns true on success. */
	UFUNCTION(BlueprintCallable, Category = "Trade")
	bool TryPurchase(class ACharacter* Buyer, const FGameplayTag& ItemTag, int32 Quantity = 1);

	/** Remove an entry from inventory (e.g., quest item handed over) */
	UFUNCTION(BlueprintCallable, Category = "Trade")
	bool RemoveFromInventory(const FGameplayTag& ItemTag, int32 Quantity = 1);

	/** Add an entry to inventory at runtime */
	UFUNCTION(BlueprintCallable, Category = "Trade")
	void AddToInventory(const FLootTradeEntry& Entry);

	/** Check if inventory contains an item with the given tag */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Trade")
	bool HasItem(const FGameplayTag& ItemTag) const;

	/** Get total quantity of an item */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Trade")
	int32 GetItemQuantity(const FGameplayTag& ItemTag) const;

	// --- Events ----------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "Trade|Events")
	FOnTradeCompleted OnTradeCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Trade|Events")
	FOnTradeOpened OnTradeOpened;

	UPROPERTY(BlueprintAssignable, Category = "Trade|Events")
	FOnTradeClosed OnTradeClosed;

protected:
	virtual void BeginPlay() override;

private:
	/** Check if a world flag is set */
	bool IsWorldFlagSet(const FGameplayTag& Flag) const;
};
