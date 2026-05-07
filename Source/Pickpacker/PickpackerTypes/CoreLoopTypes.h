// Core Loop & NPC System Types for Pickpacker

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "PickpackerTypes/PickpackerTypes.h"
#include "CoreLoopTypes.generated.h"

class UTexture2D;

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
// Detailed Run Stage
// ============================================================

/** Fine-grained run stage layered on top of the high-level phase */
UENUM(BlueprintType)
enum class ERunProgressStage : uint8
{
	None			UMETA(DisplayName = "None"),
	GameStart		UMETA(DisplayName = "Game Start"),
	Lobby			UMETA(DisplayName = "Lobby"),
	ReceiveMission	UMETA(DisplayName = "Receive Mission"),
	Work			UMETA(DisplayName = "Work"),
	Submit			UMETA(DisplayName = "Submit"),
	Underworld		UMETA(DisplayName = "Underworld"),
	Return			UMETA(DisplayName = "Return"),
	Result			UMETA(DisplayName = "Result")
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
// NPC Combat Engagement Policy
// ============================================================

UENUM(BlueprintType)
enum class ENPCEngagementPolicy : uint8
{
	Passive		UMETA(DisplayName = "Passive"),
	Defensive	UMETA(DisplayName = "Defensive"),
	Aggressive	UMETA(DisplayName = "Aggressive")
};

USTRUCT(BlueprintType)
struct PICKPACKER_API FNPCCombatSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = "0.0"))
	float AttackRange = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = "0.0"))
	float AttackDamage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = "0.0"))
	float AttackCooldown = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Policy")
	ENPCEngagementPolicy EngagementPolicy = ENPCEngagementPolicy::Defensive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Policy")
	bool bRetaliateWhenDamaged = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Policy")
	bool bBecomeAggressiveWhenDamaged = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aggro")
	bool bAutoClearTarget = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aggro")
	float LoseSightAggroGraceTime = 3.0f;

	/** Clear target when the NPC has chased this far from its initial/home location. Set <= 0 to disable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aggro")
	float MaxChaseDistanceFromHome = 2500.0f;

	/** Clear target when the target is this far from the NPC. Set <= 0 to disable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aggro")
	float MaxTargetDistance = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0.0"))
	float ChaseSpeed = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0.0"))
	float PatrolSpeed = 200.0f;
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
	AdjustPersona		UMETA(DisplayName = "Adjust Persona"),
	GiveItem			UMETA(DisplayName = "Give Item"),
	TakeItem			UMETA(DisplayName = "Take Item"),
	ChangeDisposition	UMETA(DisplayName = "Change Disposition"),
	TriggerQuest		UMETA(DisplayName = "Trigger Quest"),
	OpenTrade			UMETA(DisplayName = "Open Trade"),
	AttackPlayer		UMETA(DisplayName = "Attack Player"),
	Disappear			UMETA(DisplayName = "NPC Disappears"),
	Custom				UMETA(DisplayName = "Custom (Blueprint)")
};

UENUM(BlueprintType)
enum class EDialogueChoiceRequirementType : uint8
{
	None				UMETA(DisplayName = "None"),
	WorldFlagAtLeast	UMETA(DisplayName = "World Flag At Least"),
	WorldFlagAtMost		UMETA(DisplayName = "World Flag At Most"),
	PersonaAtLeast		UMETA(DisplayName = "Persona At Least"),
	PersonaAtMost		UMETA(DisplayName = "Persona At Most"),
	HasItemTag			UMETA(DisplayName = "Has Item Tag"),
	MissingItemTag		UMETA(DisplayName = "Missing Item Tag"),
	HasSpecialItemTag	UMETA(DisplayName = "Has Special Item Tag"),
	MissingSpecialItemTag UMETA(DisplayName = "Missing Special Item Tag")
};

UENUM(BlueprintType)
enum class EDialogueChoiceIndicatorType : uint8
{
	None		UMETA(DisplayName = "None"),
	Persona		UMETA(DisplayName = "Persona"),
	Item		UMETA(DisplayName = "Item"),
	Branch		UMETA(DisplayName = "Branch"),
	Quest		UMETA(DisplayName = "Quest"),
	Custom		UMETA(DisplayName = "Custom")
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

	/** Float payload (persona delta, weighted scores, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	float FloatValue = 0.0f;

	/** Tag payload (item tag, quest tag, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag TagPayload;

	/** Optional string payload */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FString StringPayload;
};

/** Additional requirement for unlocking a dialogue choice. All requirements must pass. */
USTRUCT(BlueprintType)
struct PICKPACKER_API FDialogueChoiceRequirement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	EDialogueChoiceRequirementType RequirementType = EDialogueChoiceRequirementType::None;

	/** World flag / item / branch tag depending on requirement type. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FGameplayTag Tag;

	/** Numeric threshold (persona or world flag threshold). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	float ThresholdValue = 0.0f;

	/** Optional indicator category shown on unlocked choices. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	EDialogueChoiceIndicatorType IndicatorType = EDialogueChoiceIndicatorType::None;

	/** Optional explicit text shown beside the indicator. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText IndicatorText;

	/** Optional explicit icon override for the indicator. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TObjectPtr<UTexture2D> IndicatorIcon = nullptr;

	/** Whether this requirement should surface an indicator when the choice is visible. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bShowIndicatorWhenMet = true;
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

	/** Additional unlock requirements such as persona threshold or item possession. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TArray<FDialogueChoiceRequirement> UnlockRequirements;

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
// NPC Persona Rule — defines how interacting affects persona
// ============================================================

/** Rule for how an NPC interaction changes persona value */
USTRUCT(BlueprintType)
struct PICKPACKER_API FNPCPersonaRule
{
	GENERATED_BODY()

	/** Item tag the player must give/use to trigger this rule (empty = any interaction) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Persona")
	FGameplayTag RequiredItemTag;

	/** Delta to apply to persona (positive = increase, negative = decrease) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Persona")
	float PersonaDelta = 0.0f;

	/** Description for debug/design */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Persona")
	FText RuleDescription;
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

	/** Combat policy values are copied into UNPCCombatComponent when the NPC initializes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Combat", meta = (EditCondition = "bEnableCombat", EditConditionHides))
	FNPCCombatSettings CombatSettings;

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

	/** Persona rules — how interactions with this NPC affect persona */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Persona")
	TArray<FNPCPersonaRule> PersonaRules;
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
	FGuid RunId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	ECoreLoopPhase CurrentPhase = ECoreLoopPhase::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	ERunProgressStage CurrentStage = ERunProgressStage::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	int32 TeamCredits = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	float TeamSuspicion = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	FMissionDefinition ActiveMission;

	/** Currently selected underground zone tag */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	FGameplayTag TargetZoneTag;

	/** Number of completed round-trips this run */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	int32 CompletedTrips = 0;

	/** Run-scoped storage persisted across phase changes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	TArray<FStorageRecord> StorageRecords;

	/** Aggregated persona state that can drive long-term branches */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	FPersonaStats PersonaStats;

	/** Ending / route flags accumulated during the run */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	TArray<FEndingFlagState> EndingFlags;
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

UENUM(BlueprintType)
enum class ETrainVoteResolutionPolicy : uint8
{
	MajorityThenHost UMETA(DisplayName = "Majority Then Host"),
	HostOnly UMETA(DisplayName = "Host Only")
};

USTRUCT(BlueprintType)
struct PICKPACKER_API FTrainDestinationVoteState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Vote")
	int32 PlayerId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Vote")
	FText PlayerName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Vote")
	FName DestinationId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Vote")
	bool bLockedIn = false;
};

USTRUCT(BlueprintType)
struct PICKPACKER_API FTrainSelectionContext
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Selection")
	TArray<FTrainDestination> AvailableDestinations;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Selection")
	TArray<FActiveOrderState> ActiveOrders;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Selection")
	FMissionDefinition MissionDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Selection")
	ETrainVoteResolutionPolicy VotePolicy = ETrainVoteResolutionPolicy::MajorityThenHost;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Selection")
	int32 EligibleVoterCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Selection")
	float BoardingDuration = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Selection")
	float DepartureDuration = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Selection")
	float TravelDuration = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Selection")
	float ArrivalDuration = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Selection")
	float AutoSelectTimeout = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Selection")
	bool bSelectionOpen = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Selection")
	FName LockedDestinationId = NAME_None;
};

USTRUCT(BlueprintType)
struct PICKPACKER_API FRouteSelectionResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Route")
	FName DestinationId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Route")
	EZoneDifficulty DangerLevel = EZoneDifficulty::Low;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Route")
	FGameplayTag TargetZoneTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Route")
	FGameplayTagContainer AvailableItemTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Route")
	int32 RouteSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Route")
	FName LevelVariantId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train|Route")
	TSoftObjectPtr<UWorld> ZoneLevel;
};
