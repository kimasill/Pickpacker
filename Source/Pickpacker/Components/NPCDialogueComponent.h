// NPCDialogueComponent - Modular dialogue module for NPC actors

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PickpackerTypes/CoreLoopTypes.h"
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
class PICKPACKER_API UNPCDialogueComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNPCDialogueComponent();

	// --- Configuration --------------------------------------------------

	/** The dialogue tree. Can be set from Data Asset or directly on the component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TArray<FDialogueNode> DialogueNodes;

	/** Speaker name fallback (used if node's SpeakerName is empty) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText DefaultSpeakerName;

	// --- Runtime State ---------------------------------------------------

	/** Whether a conversation is currently in progress */
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	bool bInConversation = false;

	/** Current dialogue node index */
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	int32 CurrentNodeIndex = -1;

	// --- API -------------------------------------------------------------

	/** Begin dialogue with a player character. Returns false if already in conversation. */
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	bool StartDialogue(class ACharacter* Interactor);

	/** Select a choice in the current node */
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void SelectChoice(class ACharacter* Interactor, int32 ChoiceIndex);

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

private:
	/** Navigate to a node index, resolving conditional skips */
	void NavigateToNode(int32 NodeIndex);

	/** Apply a dialogue outcome */
	void ApplyOutcome(class ACharacter* Interactor, const FDialogueOutcome& Outcome);

	/** Resolve the EscapeProgressComponent for world flag read/write */
	UEscapeProgressComponent* GetEscapeProgress() const;

	/** Check if a world flag is non-zero */
	bool IsWorldFlagSet(const FGameplayTag& Flag) const;
};
