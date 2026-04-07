// Core Loop & NPC System Types for Pickpacker

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "CoreLoopTypes.generated.h"

// ============================================================
// Core Loop Phase
// ============================================================

/** Phases of the core gameplay loop: Base → Train → Underground → Base */
UENUM(BlueprintType)
enum class ECoreLoopPhase : uint8
{
	None			UMETA(DisplayName = "None"),
	Base			UMETA(DisplayName = "Base - Outpost"),
	Train			UMETA(DisplayName = "Train - Transit"),
	Underground		UMETA(DisplayName = "Underground - Exploration"),
	Escape			UMETA(DisplayName = "Escape - Ending Sequence")
};

// ============================================================
// NPC Disposition / Affinity
// ============================================================

/** NPC disposition toward the player team */
UENUM(BlueprintType)
enum class ENPCDisposition : uint8
{
	Friendly	UMETA(DisplayName = "Friendly"),
	Neutral		UMETA(DisplayName = "Neutral"),
	Hostile		UMETA(DisplayName = "Hostile"),
	Missing		UMETA(DisplayName = "Missing / Gone")
};

// ============================================================
// NPC Role
// ============================================================

/** High-level role this NPC currently fulfils (can change at runtime) */
UENUM(BlueprintType)
enum class ENPCRole : uint8
{
	None		UMETA(DisplayName = "None"),
	Merchant	UMETA(DisplayName = "Merchant / Trader"),
	QuestGiver	UMETA(DisplayName = "Quest Giver"),
	Informant	UMETA(DisplayName = "Informant"),
	Guard		UMETA(DisplayName = "Guard / Hostile"),
	Companion	UMETA(DisplayName = "Companion"),
	Boss		UMETA(DisplayName = "Boss")
};

// ============================================================
// Underground Zone Difficulty
// ============================================================

/** Risk tier of an underground zone */
UENUM(BlueprintType)
enum class EZoneDifficulty : uint8
{
	Low			UMETA(DisplayName = "Low (Storage)"),
	Medium		UMETA(DisplayName = "Medium (Ruins)"),
	MediumHigh	UMETA(DisplayName = "Medium-High (Residence)"),
	High		UMETA(DisplayName = "High (Laboratory)"),
	Extreme		UMETA(DisplayName = "Extreme (Military)")
};

// ============================================================
// Dialogue Choice Outcome
// ============================================================

UENUM(BlueprintType)
enum class EDialogueOutcomeType : uint8
{
	None				UMETA(DisplayName = "None"),
	SetWorldFlag		UMETA(DisplayName = "Set World Flag"),
	GiveItem			UMETA(DisplayName = "Give Item"),
	TakeItem			UMETA(DisplayName = "Take Item"),
	ChangeDisposition	UMETA(DisplayName = "Change Disposition"),
	TriggerQuest		UMETA(DisplayName = "Trigger Quest"),
	OpenTrade			UMETA(DisplayName = "Open Trade"),
	AttackPlayer		UMETA(DisplayName = "Attack Player"),
	Disappear			UMETA(DisplayName = "NPC Disappears"),
	Custom				UMETA(DisplayName = "Custom (Blueprint)")
};

// ============================================================
// Dialogue Structs
// ============================================================

/** Single outcome triggered by a dialogue choice */
USTRUCT(BlueprintType)
struct PICKPACKER_API FDialogueOutcome
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	EDialogueOutcomeType OutcomeType = EDialogueOutcomeType::None;

	/** World flag to set / check */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (EditCondition = "OutcomeType == EDialogueOutcomeType::SetWorldFlag"))
	FGameplayTag WorldFlag;

	/** Integer payload (flag value, item quantity, disposition delta, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	int32 IntValue = 0;

	/** Tag payload (item tag, quest tag, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag TagPayload;

	/** Optional string payload */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FString StringPayload;
};

/** One selectable choice inside a dialogue node */
USTRUCT(BlueprintType)
struct PICKPACKER_API FDialogueChoice
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText ChoiceText;

	/** Condition: choice is only shown if this world flag is non-zero (empty = always shown) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag RequiredWorldFlag;

	/** Condition: choice hidden if this flag is non-zero */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag BlockingWorldFlag;

	/** Outcomes triggered when this choice is selected */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TArray<FDialogueOutcome> Outcomes;

	/** Index of the next dialogue node to jump to (-1 = end conversation) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	int32 NextNodeIndex = -1;
};

/** A single dialogue node (NPC speech + player choices) */
USTRUCT(BlueprintType)
struct PICKPACKER_API FDialogueNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText SpeakerName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText DialogueText;

	/** Condition: this node is skipped if the flag is zero (empty = always show) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag RequiredWorldFlag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TArray<FDialogueChoice> Choices;

	/** If no choices, auto-advance to this node index (-1 = end) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	int32 AutoNextNodeIndex = -1;
};

// ============================================================
// Loot / Trade Entry
// ============================================================

USTRUCT(BlueprintType)
struct PICKPACKER_API FLootTradeEntry
{
	GENERATED_BODY()

	/** Tag identifying the item / parcel */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	FGameplayTag ItemTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	int32 Quantity = 1;

	/** Credit cost (0 = free, negative = NPC buys from player) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	int32 CreditCost = 0;

	/** World flag required for this entry to appear (empty = always) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	FGameplayTag RequiredWorldFlag;
};

// ============================================================
// NPC Profile (Data-driven NPC configuration)
// ============================================================

USTRUCT(BlueprintType)
struct PICKPACKER_API FNPCProfile
{
	GENERATED_BODY()

	/** Unique NPC identifier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
	FName NPCId = NAME_None;

	/** Display name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
	FText DisplayName;

	/** Default disposition */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
	ENPCDisposition DefaultDisposition = ENPCDisposition::Neutral;

	/** Default role */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
	ENPCRole DefaultRole = ENPCRole::None;

	/** Zone where this NPC spawns */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
	EZoneDifficulty HomeZone = EZoneDifficulty::Low;

	/** Enable dialogue module */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Modules")
	bool bEnableDialogue = true;

	/** Enable combat module */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Modules")
	bool bEnableCombat = false;

	/** Enable loot/trade module */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Modules")
	bool bEnableLootTrade = false;

	/** Dialogue tree */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Dialogue")
	TArray<FDialogueNode> DialogueNodes;

	/** Trade / loot inventory */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Loot")
	TArray<FLootTradeEntry> TradeInventory;

	/** World flag: if non-zero and flag value matches, NPC is Missing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Narrative")
	FGameplayTag DisappearFlag;

	/** World flag that changes this NPC to Hostile */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Narrative")
	FGameplayTag HostileFlag;
};

// ============================================================
// Run State (volatile, per-session)
// ============================================================

USTRUCT(BlueprintType)
struct PICKPACKER_API FRunState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	bool bRunActive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	ECoreLoopPhase CurrentPhase = ECoreLoopPhase::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	int32 TeamCredits = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	float TeamSuspicion = 0.0f;

	/** Currently selected underground zone tag */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	FGameplayTag TargetZoneTag;

	/** Number of completed round-trips this run */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	int32 CompletedTrips = 0;
};

// ============================================================
// Train Destination
// ============================================================

USTRUCT(BlueprintType)
struct PICKPACKER_API FTrainDestination
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train")
	FName DestinationId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train")
	EZoneDifficulty Difficulty = EZoneDifficulty::Low;

	/** Tags describing what items can be farmed here */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train")
	FGameplayTagContainer AvailableItemTags;

	/** World flag required to unlock this destination (empty = always available) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train")
	FGameplayTag UnlockFlag;

	/** Level path to load for this zone (streaming or travel) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train")
	TSoftObjectPtr<UWorld> ZoneLevel;
};
