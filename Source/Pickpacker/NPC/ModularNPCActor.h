// ModularNPCActor - Base actor for all modular NPCs
// Can act as friendly trader, quest giver, hostile enemy, or disappear based on world state

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PickpackerTypes/CoreLoopTypes.h"
#include "Interfaces/InteractableInterface.h"
#include "UI/NPCDialogueUIData.h"
#include "ModularNPCActor.generated.h"

class UNPCDialogueComponent;
class UNPCCombatComponent;
class UNPCLootTradeComponent;
class UNPCModuleComponent;
class UDA_NPCData;
class UWidgetComponent;
class UEscapeProgressComponent;
class AAIController;
class UBlackboardComponent;
class UBehaviorTree;
class UBlackboardData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNPCDispositionChanged, ENPCDisposition, OldDisposition, ENPCDisposition, NewDisposition);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNPCRoleChanged, ENPCRole, OldRole, ENPCRole, NewRole);

/**
 * Modular NPC Actor – one actor class, many behaviours.
 *
 * The same blueprint/actor base can serve as:
 *  - A friendly trader (Dialogue + LootTrade active)
 *  - A quest-giver NPC (Dialogue active)
 *  - A hostile enemy (Combat active)
 *  - Missing / gone (hidden, no collision)
 *
 * Behaviour is driven by:
 *  1. DA_NPCData (data asset, sets defaults)
 *  2. World flags (runtime narrative state changes disposition / visibility)
 *  3. Direct API calls (e.g., from dialogue outcomes)
 */
UCLASS(BlueprintType, Blueprintable)
class PICKPACKER_API AModularNPCActor : public ACharacter, public IInteractableInterface
{
	GENERATED_BODY()

public:
	AModularNPCActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// --- IInteractableInterface ------------------------------------------

	virtual void OnInteract_Implementation(ACharacter* Interactor) override;
	virtual bool CanInteract_Implementation(ACharacter* Interactor) override;
	virtual FText GetInteractText_Implementation() override;
	virtual void GetInteractionUIData_Implementation(FInteractionUIData& OutData) override;
	virtual void StartHighlight_Implementation() override;
	virtual void EndHighlight_Implementation() override;
	virtual bool RequestShowInteractionUI_Implementation(ACharacter* Interactor) override;
	virtual void GetCreditUnlockInfo_Implementation(bool& bRequiresUnlock, int32& UnlockCost, FText& LockedMessage, FText& UnlockedMessage) override;

	// --- Configuration ---------------------------------------------------

	/** Data asset driving this NPC's profile */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
	UDA_NPCData* NPCData = nullptr;

	/** Unique NPC ID (overrides data asset if set) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC")
	FName NPCId = NAME_None;

	// --- Runtime State ---------------------------------------------------

	/** Current disposition */
	UPROPERTY(ReplicatedUsing = OnRep_Disposition, EditAnywhere, BlueprintReadWrite, Category = "NPC|State")
	ENPCDisposition Disposition = ENPCDisposition::Neutral;

	/** Current role */
	UPROPERTY(ReplicatedUsing = OnRep_Role, EditAnywhere, BlueprintReadWrite, Category = "NPC|State")
	ENPCRole NPCRole = ENPCRole::None;

	/** Display name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|State")
	FText DisplayName;

	// --- Components (modular) --------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC|Components")
	UNPCDialogueComponent* DialogueComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC|Components")
	UNPCCombatComponent* CombatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC|Components")
	UNPCLootTradeComponent* LootTradeComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NPC|Components")
	UWidgetComponent* NameWidget;

	// --- API -------------------------------------------------------------

	/** Change disposition at runtime. Automatically activates/deactivates modules. */
	UFUNCTION(BlueprintCallable, Category = "NPC")
	void SetDisposition(ENPCDisposition NewDisposition);

	/** Change role at runtime */
	UFUNCTION(BlueprintCallable, Category = "NPC")
	void SetNPCRole(ENPCRole NewRole);

	/** Evaluate world flags and update disposition accordingly */
	UFUNCTION(BlueprintCallable, Category = "NPC")
	void EvaluateNarrativeState();

	/** Make the NPC disappear (Missing state) */
	UFUNCTION(BlueprintCallable, Category = "NPC")
	void Disappear();

	/** Make the NPC reappear */
	UFUNCTION(BlueprintCallable, Category = "NPC")
	void Reappear();

	/** Get the effective NPC ID (from data asset or override) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NPC")
	FName GetEffectiveNPCId() const;

	bool StartDialogueForInteractor(class ACharacter* Interactor);
	bool SelectDialogueChoiceForInteractor(class ACharacter* Interactor, int32 ChoiceIndex);
	bool AdvanceDialogueForInteractor(class ACharacter* Interactor);
	void EndDialogueForInteractor(class ACharacter* Interactor);
	bool BuildDialogueUIState(FDialogueUIState& OutDialogueState) const;

	// --- Events ----------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "NPC|Events")
	FOnNPCDispositionChanged OnDispositionChanged;

	UPROPERTY(BlueprintAssignable, Category = "NPC|Events")
	FOnNPCRoleChanged OnRoleChanged;

protected:
	UFUNCTION()
	void OnRep_Disposition(ENPCDisposition OldDisposition);

	UFUNCTION()
	void OnRep_Role(ENPCRole OldNPCRole);

	/** Initialize from NPCData */
	void InitializeFromData();
	void EnsureDialogueDataInitialized();

	/** Activate/deactivate modules based on current disposition */
	void UpdateModuleStates();

	/** Ensure CharacterMovement is in a valid runtime mode. */
	void EnsureMovementModeInitialized();

	/** Refresh and notify all attached module components */
	void RefreshModuleComponents();
	void NotifyModulesOwnerReady();
	void NotifyModulesDispositionChanged(ENPCDisposition OldDisposition, ENPCDisposition NewDisposition);
	void NotifyModulesRoleChanged(ENPCRole OldRole, ENPCRole NewRole);
	void NotifyModulesNarrativeStateEvaluated();
	void NotifyModulesUpdated();

	/** Start or refresh combat AI when the NPC becomes hostile */
	void StartCombatAI();

	/** Start or refresh ambient AI for non-hostile mobile NPCs */
	void StartAmbientAI();

	/** Stop combat AI when leaving hostile state */
	void StopCombatAI();

	/** Ensure the NPC has an AI controller for combat */
	AAIController* EnsureCombatAIController();

	/** Keep combat-related blackboard keys in sync */
	void SyncCombatBlackboard();

	/** Clear combat-related blackboard keys */
	void ClearCombatBlackboard();

	UBehaviorTree* ResolveCombatBehaviorTree() const;
	UBlackboardData* ResolveCombatBlackboard() const;
	UBehaviorTree* ResolveAmbientBehaviorTree() const;
	UBlackboardData* ResolveAmbientBlackboard() const;

	/** Get EscapeProgressComponent for world flag queries */
	UEscapeProgressComponent* GetEscapeProgress() const;

	/** Dialogue choice handler – listens for outcomes that affect the NPC */
	UFUNCTION()
	void OnDialogueChoiceMade(ACharacter* Interactor, int32 NodeIndex, int32 ChoiceIndex);

	UFUNCTION()
	void OnDialogueEnded();

	UFUNCTION()
	void OnCombatTargetAcquired(AActor* NewTarget);

	UFUNCTION()
	void OnCombatTargetLost();

	UFUNCTION()
	void OnCombatDied();

private:
	/** Timer for periodic narrative state evaluation */
	FTimerHandle NarrativeEvalTimerHandle;

	/** Interval for evaluating world flags (seconds) */
	UPROPERTY(EditAnywhere, Category = "NPC|Narrative")
	float NarrativeEvalInterval = 5.0f;

	/** Cached profile from data asset */
	FNPCProfile CachedProfile;
	bool bProfileCached = false;

	/** Cached home location for hostile BTs that need return logic */
	FVector HomeLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNPCModuleComponent>> RegisteredModules;

	UPROPERTY(Transient)
	TWeakObjectPtr<class ACharacter> ActiveDialogueInteractor;
};
