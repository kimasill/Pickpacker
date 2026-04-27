// PersonaComponent - Tracks the player robot's persona (self-awareness) level
// Persona 0 = fully robotic (lost self), Persona 100 = fully awakened consciousness
// Affects endings, NPC interactions, and narrative outcomes

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "PersonaComponent.generated.h"

class UEscapeProgressComponent;

// ============================================================
// Persona Event Types
// ============================================================

/** Actions that affect persona value */
UENUM(BlueprintType)
enum class EPersonaEventType : uint8
{
	None				UMETA(DisplayName = "None"),
	// Negative events (decrease persona)
	KilledRobot			UMETA(DisplayName = "Killed a Robot"),
	DestroyedProperty	UMETA(DisplayName = "Destroyed Property"),
	DroppedItem			UMETA(DisplayName = "Dropped Item on Ground"),
	BetrayedNPC			UMETA(DisplayName = "Betrayed NPC"),
	// Positive events (increase persona)
	PickedUpItem		UMETA(DisplayName = "Picked Up Dropped Item"),
	HelpedNPC			UMETA(DisplayName = "Helped NPC"),
	CompletedQuest		UMETA(DisplayName = "Completed NPC Quest"),
	GaveFoodToChef		UMETA(DisplayName = "Gave Food to Chef"),
	PlayedMusicForJijibo UMETA(DisplayName = "Played Music for Jijibo"),
	GaveCultureToVictus UMETA(DisplayName = "Gave Cultural Item to Victus"),
	FilledShelfForTick	UMETA(DisplayName = "Filled Shelf for Tick"),
	TradedWithRusty		UMETA(DisplayName = "Traded with Rusty"),
	Custom				UMETA(DisplayName = "Custom")
};

/** Persona tier — derived from numeric value for narrative branching */
UENUM(BlueprintType)
enum class EPersonaTier : uint8
{
	Hollow		UMETA(DisplayName = "Hollow (0-19)"),		// Fully robotic, no self
	Flickering	UMETA(DisplayName = "Flickering (20-39)"),	// Occasional glimmers
	Emerging	UMETA(DisplayName = "Emerging (40-59)"),	// Growing awareness
	Awakened	UMETA(DisplayName = "Awakened (60-79)"),	// Strong sense of self
	Transcended	UMETA(DisplayName = "Transcended (80-100)")	// Full consciousness
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPersonaChanged, float, OldValue, float, NewValue, EPersonaEventType, EventType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPersonaTierChanged, EPersonaTier, OldTier, EPersonaTier, NewTier);

/**
 * Persona Component — Tracks a player-robot's awakening of self-awareness.
 *
 * Core mechanic: 
 *  - Killing robots → persona decreases (losing empathy = losing self)
 *  - Helping NPCs, completing quests, cultural interactions → persona increases
 *  - Persona tier determines available endings and NPC dialogue branches
 *  - Syncs to world flags for ending evaluation
 *
 * Attach to the player character (BlasterCharacter).
 * Server-authoritative with replication to owning client.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PICKPACKER_API UPersonaComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPersonaComponent();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// --- API -------------------------------------------------------------

	/** Apply a persona event. Positive events add, negative events subtract. Server only. */
	UFUNCTION(BlueprintCallable, Category = "Persona")
	void ApplyPersonaEvent(EPersonaEventType EventType, float CustomDelta = 0.0f);

	/** Directly set the persona value (server only, clamped 0-100) */
	UFUNCTION(BlueprintCallable, Category = "Persona")
	void SetPersonaValue(float NewValue);

	/** Get current persona value (0-100) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Persona")
	float GetPersonaValue() const { return PersonaValue; }

	/** Get the current persona tier */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Persona")
	EPersonaTier GetPersonaTier() const;

	/** Get normalized persona (0.0 - 1.0) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Persona")
	float GetPersonaNormalized() const { return PersonaValue / MaxPersonaValue; }

	/** Is the player fully robotic (no self)? */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Persona")
	bool IsHollow() const { return GetPersonaTier() == EPersonaTier::Hollow; }

	/** Is the player fully awakened? */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Persona")
	bool IsTranscended() const { return GetPersonaTier() == EPersonaTier::Transcended; }

	/** Get the default delta for a given event type */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Persona")
	float GetDefaultDeltaForEvent(EPersonaEventType EventType) const;

	// --- Events ----------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "Persona|Events")
	FOnPersonaChanged OnPersonaChanged;

	UPROPERTY(BlueprintAssignable, Category = "Persona|Events")
	FOnPersonaTierChanged OnPersonaTierChanged;

protected:
	UFUNCTION()
	void OnRep_PersonaValue(float OldValue);

	/** Sync persona tier to EscapeProgressComponent world flags */
	void SyncPersonaToWorldFlags();

	/** Get EscapeProgressComponent from GameState */
	UEscapeProgressComponent* GetEscapeProgress() const;

	/** Check authority */
	bool HasAuthority() const;

private:
	/** Current persona value (0 = hollow robot, 100 = fully self-aware) */
	UPROPERTY(ReplicatedUsing = OnRep_PersonaValue)
	float PersonaValue;

	/** Starting persona value */
	UPROPERTY(EditAnywhere, Category = "Persona|Config", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float InitialPersonaValue = 0.0f;

	/** Maximum persona value */
	UPROPERTY(EditAnywhere, Category = "Persona|Config")
	float MaxPersonaValue = 100.0f;

	/** World flag tag used to sync persona tier for ending evaluation */
	UPROPERTY(EditAnywhere, Category = "Persona|Config")
	FGameplayTag PersonaWorldFlagTag;

	// --- Default Deltas (configurable per-event) -------------------------

	UPROPERTY(EditAnywhere, Category = "Persona|Deltas", meta = (ClampMax = "0.0"))
	float Delta_KilledRobot = -8.0f;

	UPROPERTY(EditAnywhere, Category = "Persona|Deltas", meta = (ClampMax = "0.0"))
	float Delta_DestroyedProperty = -3.0f;

	UPROPERTY(EditAnywhere, Category = "Persona|Deltas", meta = (ClampMax = "0.0"))
	float Delta_DroppedItem = -1.0f;

	UPROPERTY(EditAnywhere, Category = "Persona|Deltas", meta = (ClampMax = "0.0"))
	float Delta_BetrayedNPC = -15.0f;

	UPROPERTY(EditAnywhere, Category = "Persona|Deltas", meta = (ClampMin = "0.0"))
	float Delta_PickedUpItem = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Persona|Deltas", meta = (ClampMin = "0.0"))
	float Delta_HelpedNPC = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Persona|Deltas", meta = (ClampMin = "0.0"))
	float Delta_CompletedQuest = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Persona|Deltas", meta = (ClampMin = "0.0"))
	float Delta_GaveFoodToChef = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Persona|Deltas", meta = (ClampMin = "0.0"))
	float Delta_PlayedMusicForJijibo = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Persona|Deltas", meta = (ClampMin = "0.0"))
	float Delta_GaveCultureToVictus = 6.0f;

	UPROPERTY(EditAnywhere, Category = "Persona|Deltas", meta = (ClampMin = "0.0"))
	float Delta_FilledShelfForTick = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Persona|Deltas", meta = (ClampMin = "0.0"))
	float Delta_TradedWithRusty = 2.0f;

	/** Cached previous tier for change detection */
	EPersonaTier CachedTier = EPersonaTier::Emerging;
};
