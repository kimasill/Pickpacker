// NPCDialogueComponent - Modular dialogue module for NPC actors

#pragma once

#include "CoreMinimal.h"
#include "Components/NPCModuleComponent.h"
#include "Interfaces/InteractableInterface.h"
#include "PickpackerTypes/CoreLoopTypes.h"
#include "UI/NPCDialogueUIData.h"
#include "NPCDialogueComponent.generated.h"

class UEscapeProgressComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDialogueStarted, class ACharacter*, Interactor, int32, NodeIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDialogueEnded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDialogueChoiceSelected, class ACharacter*, Interactor, int32, NodeIndex, int32, ChoiceIndex);

/**
 * Modular dialogue component. Attach to any NPC actor to give it conversation capability.
 * Reads a dialogue tree (TArray<FDialogueNode>) and evaluates world-flag conditions
 * to drive branching narratives.
 */
UCLASS(ClassGroup = (NPC), meta = (BlueprintSpawnableComponent))
class PICKPACKER_API UNPCDialogueComponent : public UNPCModuleComponent, public IInteractableInterface
{
	GENERATED_BODY()

public:
	UNPCDialogueComponent();

	virtual void OnInteract_Implementation(class ACharacter* Interactor) override;
	virtual bool CanInteract_Implementation(class ACharacter* Interactor) override;
	virtual FText GetInteractText_Implementation() override;
	virtual void GetInteractionUIData_Implementation(FInteractionUIData& OutData) override;
	virtual bool RequestShowInteractionUI_Implementation(class ACharacter* Interactor) override;
	virtual void GetCreditUnlockInfo_Implementation(bool& bRequiresUnlock, int32& UnlockCost, FText& LockedMessage, FText& UnlockedMessage) override;

	// --- Configuration --------------------------------------------------

	/** The dialogue tree. Can be set from Data Asset or directly on the component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TArray<FDialogueNode> DialogueNodes;

	/** Speaker name fallback (used if node's SpeakerName is empty) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText DefaultSpeakerName;

	// --- Runtime State ---------------------------------------------------

	/** Whether a conversation is currently in progress */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Dialogue")
	bool bInConversation = false;

	/** Current dialogue node index */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Dialogue")
	int32 CurrentNodeIndex = -1;

	// --- API -------------------------------------------------------------

	/** Begin dialogue with a player character. Returns false if already in conversation. */
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	bool StartDialogue(class ACharacter* Interactor);

	bool StartDialogueForInteractor(class ACharacter* Interactor);
	bool SelectChoiceForInteractor(class ACharacter* Interactor, int32 ChoiceIndex);
	bool AdvanceDialogueForInteractor(class ACharacter* Interactor);
	void EndDialogueForInteractor(class ACharacter* Interactor);
	bool BuildDialogueUIState(FDialogueUIState& OutDialogueState) const;

	/** Select a choice in the current node */
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void SelectChoice(class ACharacter* Interactor, int32 ChoiceIndex);

	/** Advance dialogue when the current node has no player choices */
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void AdvanceDialogue(class ACharacter* Interactor);

	/** End the dialogue early */
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void EndDialogue();

	/** Get the current node (valid only during conversation) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Dialogue")
	bool GetCurrentNode(FDialogueNode& OutNode) const;

	/** Get available choices for the current node (filtered by world flags) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Dialogue")
	TArray<FDialogueChoice> GetAvailableChoices() const;

	// --- Events ----------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "Dialogue|Events")
	FOnDialogueStarted OnDialogueStarted;

	UPROPERTY(BlueprintAssignable, Category = "Dialogue|Events")
	FOnDialogueEnded OnDialogueEnded;

	UPROPERTY(BlueprintAssignable, Category = "Dialogue|Events")
	FOnDialogueChoiceSelected OnDialogueChoiceSelected;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** Navigate to a node index, resolving conditional skips */
	void NavigateToNode(int32 NodeIndex);

	/** Apply a dialogue outcome */
	void ApplyOutcome(class ACharacter* Interactor, const FDialogueOutcome& Outcome);

	/** Resolve the EscapeProgressComponent for world flag read/write */
	UEscapeProgressComponent* GetEscapeProgress() const;

	/** Check if a world flag is non-zero */
	bool IsWorldFlagSet(const FGameplayTag& Flag) const;

	int32 GetWorldFlagValue(const FGameplayTag& Flag) const;
	float GetInteractorPersonaValue(class ACharacter* Interactor) const;
	bool DoesInteractorHaveItemTag(class ACharacter* Interactor, const FGameplayTag& ItemTag, bool bSpecialItemOnly) const;
	bool DoesChoiceRequirementPass(const FDialogueChoiceRequirement& Requirement, class ACharacter* Interactor) const;
	bool DoesChoicePassRequirements(const FDialogueChoice& Choice, class ACharacter* Interactor) const;
	bool BuildChoiceUIData(const FDialogueChoice& Choice, int32 ChoiceIndex, class ACharacter* Interactor, FDialogueChoiceUIData& OutChoiceUIData) const;
	bool DoesRequirementHaveIndicator(const FDialogueChoiceRequirement& Requirement) const;
	FText BuildRequirementIndicatorText(const FDialogueChoiceRequirement& Requirement) const;
	FText GetGameplayTagDisplayText(const FGameplayTag& Tag) const;
	bool ConsumeInteractorItemTag(class ACharacter* Interactor, const FGameplayTag& ItemTag) const;

	void PushDialogueStateToInteractor(class ACharacter* Interactor);

	UPROPERTY(Transient)
	TWeakObjectPtr<class ACharacter> ActiveInteractor;
};
