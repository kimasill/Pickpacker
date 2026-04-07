// NPCDialogueComponent.cpp

#include "NPCDialogueComponent.h"
#include "GameFramework/Character.h"
#include "Components/EscapeProgressComponent.h"
#include "GameState/PickpackerGameState.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

UNPCDialogueComponent::UNPCDialogueComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UNPCDialogueComponent::BeginPlay()
{
	Super::BeginPlay();
}

// =========================================================================
// API
// =========================================================================

bool UNPCDialogueComponent::StartDialogue(ACharacter* Interactor)
{
	if (bInConversation || DialogueNodes.Num() == 0 || !Interactor)
	{
		return false;
	}

	bInConversation = true;
	NavigateToNode(0);

	OnDialogueStarted.Broadcast(Interactor, CurrentNodeIndex);
	return true;
}

void UNPCDialogueComponent::SelectChoice(ACharacter* Interactor, int32 ChoiceIndex)
{
	if (!bInConversation || !DialogueNodes.IsValidIndex(CurrentNodeIndex))
	{
		return;
	}

	const FDialogueNode& Node = DialogueNodes[CurrentNodeIndex];
	TArray<FDialogueChoice> Available = GetAvailableChoices();

	if (!Available.IsValidIndex(ChoiceIndex))
	{
		return;
	}

	const FDialogueChoice& Choice = Available[ChoiceIndex];

	// Broadcast before applying outcomes so listeners can inspect pre-state
	OnDialogueChoiceSelected.Broadcast(Interactor, CurrentNodeIndex, ChoiceIndex);

	// Apply all outcomes
	for (const FDialogueOutcome& Outcome : Choice.Outcomes)
	{
		ApplyOutcome(Interactor, Outcome);
	}

	// Navigate
	if (Choice.NextNodeIndex < 0)
	{
		EndDialogue();
	}
	else
	{
		NavigateToNode(Choice.NextNodeIndex);
	}
}

void UNPCDialogueComponent::EndDialogue()
{
	if (!bInConversation)
	{
		return;
	}

	bInConversation = false;
	CurrentNodeIndex = -1;

	OnDialogueEnded.Broadcast();
}

bool UNPCDialogueComponent::GetCurrentNode(FDialogueNode& OutNode) const
{
	if (!bInConversation || !DialogueNodes.IsValidIndex(CurrentNodeIndex))
	{
		return false;
	}
	OutNode = DialogueNodes[CurrentNodeIndex];
	return true;
}

TArray<FDialogueChoice> UNPCDialogueComponent::GetAvailableChoices() const
{
	TArray<FDialogueChoice> Result;
	if (!bInConversation || !DialogueNodes.IsValidIndex(CurrentNodeIndex))
	{
		return Result;
	}

	const FDialogueNode& Node = DialogueNodes[CurrentNodeIndex];
	for (const FDialogueChoice& Choice : Node.Choices)
	{
		// Check required flag
		if (Choice.RequiredWorldFlag.IsValid() && !IsWorldFlagSet(Choice.RequiredWorldFlag))
		{
			continue;
		}
		// Check blocking flag
		if (Choice.BlockingWorldFlag.IsValid() && IsWorldFlagSet(Choice.BlockingWorldFlag))
		{
			continue;
		}
		Result.Add(Choice);
	}
	return Result;
}

// =========================================================================
// Internal
// =========================================================================

void UNPCDialogueComponent::NavigateToNode(int32 NodeIndex)
{
	// Resolve conditional skips (max 64 hops to prevent infinite loops)
	for (int32 Safety = 0; Safety < 64; ++Safety)
	{
		if (!DialogueNodes.IsValidIndex(NodeIndex))
		{
			EndDialogue();
			return;
		}

		const FDialogueNode& Node = DialogueNodes[NodeIndex];

		// Check condition
		if (Node.RequiredWorldFlag.IsValid() && !IsWorldFlagSet(Node.RequiredWorldFlag))
		{
			// Skip to auto-next or end
			if (Node.AutoNextNodeIndex >= 0)
			{
				NodeIndex = Node.AutoNextNodeIndex;
				continue;
			}
			EndDialogue();
			return;
		}

		// Valid node
		CurrentNodeIndex = NodeIndex;

		// Auto-advance if no choices
		if (Node.Choices.Num() == 0 && Node.AutoNextNodeIndex >= 0)
		{
			// For auto-advance we still stay on this node;
			// UI should call SelectChoice or a timer to advance.
		}
		return;
	}

	EndDialogue();
}

void UNPCDialogueComponent::ApplyOutcome(ACharacter* Interactor, const FDialogueOutcome& Outcome)
{
	UEscapeProgressComponent* EscapeProgress = GetEscapeProgress();

	switch (Outcome.OutcomeType)
	{
	case EDialogueOutcomeType::SetWorldFlag:
		if (EscapeProgress && Outcome.WorldFlag.IsValid())
		{
			EscapeProgress->SetWorldFlag(Outcome.WorldFlag, Outcome.IntValue != 0 ? Outcome.IntValue : 1);
		}
		break;

	case EDialogueOutcomeType::ChangeDisposition:
		// Handled by the owning NPC actor via OnDialogueChoiceSelected
		break;

	case EDialogueOutcomeType::Disappear:
		if (AActor* Owner = GetOwner())
		{
			Owner->SetActorHiddenInGame(true);
			Owner->SetActorEnableCollision(false);
			Owner->SetActorTickEnabled(false);
		}
		break;

	case EDialogueOutcomeType::GiveItem:
	case EDialogueOutcomeType::TakeItem:
	case EDialogueOutcomeType::TriggerQuest:
	case EDialogueOutcomeType::OpenTrade:
	case EDialogueOutcomeType::AttackPlayer:
	case EDialogueOutcomeType::Custom:
		// These are handled by the owning NPC actor listening to OnDialogueChoiceSelected
		break;

	default:
		break;
	}
}

UEscapeProgressComponent* UNPCDialogueComponent::GetEscapeProgress() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	AGameStateBase* GS = World->GetGameState();
	APickpackerGameState* PickpackerGS = Cast<APickpackerGameState>(GS);
	if (PickpackerGS)
	{
		return PickpackerGS->GetEscapeProgressComponent();
	}
	return nullptr;
}

bool UNPCDialogueComponent::IsWorldFlagSet(const FGameplayTag& Flag) const
{
	UEscapeProgressComponent* EscapeProgress = GetEscapeProgress();
	if (!EscapeProgress || !Flag.IsValid())
	{
		return false;
	}
	return EscapeProgress->GetWorldFlag(Flag) != 0;
}
