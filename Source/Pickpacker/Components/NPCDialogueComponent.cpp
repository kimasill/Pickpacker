// NPCDialogueComponent.cpp

#include "NPCDialogueComponent.h"
#include "Character/BlasterCharacter.h"
#include "GameFramework/Character.h"
#include "Components/EscapeProgressComponent.h"
#include "Components/InteractionComponent.h"
#include "Components/PersonaComponent.h"
#include "Components/PlayerInventoryComponent.h"
#include "GameState/PickpackerGameState.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Interaction/InteractionUIData.h"
#include "Parcel/ParcelActor.h"
#include "PlayerController/BlasterPlayerController.h"
#include "Net/UnrealNetwork.h"

UNPCDialogueComponent::UNPCDialogueComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UNPCDialogueComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UNPCDialogueComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UNPCDialogueComponent, bInConversation);
	DOREPLIFETIME(UNPCDialogueComponent, CurrentNodeIndex);
}

void UNPCDialogueComponent::OnInteract_Implementation(ACharacter* Interactor)
{
	StartDialogueForInteractor(Interactor);
}

bool UNPCDialogueComponent::CanInteract_Implementation(ACharacter* Interactor)
{
	return Interactor != nullptr && DialogueNodes.Num() > 0;
}

FText UNPCDialogueComponent::GetInteractText_Implementation()
{
	if (!DefaultSpeakerName.IsEmpty())
	{
		return FText::Format(NSLOCTEXT("NPCDialogue", "TalkToSpeaker", "Talk to {0}"), DefaultSpeakerName);
	}

	if (AActor* OwnerActor = GetOwner())
	{
		return FText::Format(NSLOCTEXT("NPCDialogue", "TalkToOwner", "Talk to {0}"), FText::FromString(OwnerActor->GetName()));
	}

	return NSLOCTEXT("NPCDialogue", "TalkFallback", "Talk");
}

void UNPCDialogueComponent::GetInteractionUIData_Implementation(FInteractionUIData& OutData)
{
	OutData.InteractionType = EInteractionType::Talk;
	OutData.ActionText = GetInteractText_Implementation();
	OutData.DetailText = DefaultSpeakerName;
}

bool UNPCDialogueComponent::RequestShowInteractionUI_Implementation(ACharacter* Interactor)
{
	return false;
}

void UNPCDialogueComponent::GetCreditUnlockInfo_Implementation(bool& bRequiresUnlock, int32& UnlockCost, FText& LockedMessage, FText& UnlockedMessage)
{
	bRequiresUnlock = false;
	UnlockCost = 0;
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
	ActiveInteractor = Interactor;
	NavigateToNode(0);
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}

	OnDialogueStarted.Broadcast(Interactor, CurrentNodeIndex);
	return true;
}

bool UNPCDialogueComponent::StartDialogueForInteractor(ACharacter* Interactor)
{
	if (!StartDialogue(Interactor))
	{
		if (bInConversation && ActiveInteractor.Get() == Interactor)
		{
			PushDialogueStateToInteractor(Interactor);
		}
		return bInConversation && ActiveInteractor.Get() == Interactor;
	}

	PushDialogueStateToInteractor(Interactor);
	return true;
}

bool UNPCDialogueComponent::SelectChoiceForInteractor(ACharacter* Interactor, int32 ChoiceIndex)
{
	if (!bInConversation || ActiveInteractor.Get() != Interactor)
	{
		return false;
	}

	SelectChoice(Interactor, ChoiceIndex);
	if (bInConversation)
	{
		PushDialogueStateToInteractor(Interactor);
	}
	else if (ABlasterPlayerController* BlasterPC = Cast<ABlasterPlayerController>(Interactor ? Interactor->GetController() : nullptr))
	{
		BlasterPC->ClientCloseNPCDialogue(GetOwner());
	}

	return true;
}

bool UNPCDialogueComponent::AdvanceDialogueForInteractor(ACharacter* Interactor)
{
	if (!bInConversation || ActiveInteractor.Get() != Interactor)
	{
		return false;
	}

	AdvanceDialogue(Interactor);
	if (bInConversation)
	{
		PushDialogueStateToInteractor(Interactor);
	}
	else if (ABlasterPlayerController* BlasterPC = Cast<ABlasterPlayerController>(Interactor ? Interactor->GetController() : nullptr))
	{
		BlasterPC->ClientCloseNPCDialogue(GetOwner());
	}

	return true;
}

void UNPCDialogueComponent::EndDialogueForInteractor(ACharacter* Interactor)
{
	if (!bInConversation)
	{
		return;
	}

	if (Interactor && ActiveInteractor.Get() != Interactor)
	{
		return;
	}

	EndDialogue();
}

bool UNPCDialogueComponent::BuildDialogueUIState(FDialogueUIState& OutDialogueState) const
{
	if (!bInConversation || !DialogueNodes.IsValidIndex(CurrentNodeIndex))
	{
		return false;
	}

	const FDialogueNode& CurrentNode = DialogueNodes[CurrentNodeIndex];

	OutDialogueState = FDialogueUIState();
	OutDialogueState.SpeakerName = CurrentNode.SpeakerName.IsEmpty() ? DefaultSpeakerName : CurrentNode.SpeakerName;
	OutDialogueState.DialogueText = CurrentNode.DialogueText;
	OutDialogueState.bCanExit = true;

	for (int32 ChoiceIndex = 0; ChoiceIndex < CurrentNode.Choices.Num(); ++ChoiceIndex)
	{
		FDialogueChoiceUIData ChoiceUIData;
		if (!BuildChoiceUIData(CurrentNode.Choices[ChoiceIndex], ChoiceIndex, ActiveInteractor.Get(), ChoiceUIData))
		{
			continue;
		}

		OutDialogueState.Choices.Add(ChoiceUIData);
	}

	OutDialogueState.bCanAdvance = CurrentNode.AutoNextNodeIndex >= 0 && CurrentNode.Choices.Num() == 0;
	return true;
}

void UNPCDialogueComponent::SelectChoice(ACharacter* Interactor, int32 ChoiceIndex)
{
	if (!bInConversation || !DialogueNodes.IsValidIndex(CurrentNodeIndex))
	{
		return;
	}

	const FDialogueNode& Node = DialogueNodes[CurrentNodeIndex];
	if (!Node.Choices.IsValidIndex(ChoiceIndex))
	{
		return;
	}

	const FDialogueChoice& Choice = Node.Choices[ChoiceIndex];
	if (!DoesChoicePassRequirements(Choice, Interactor))
	{
		return;
	}

	// Broadcast before applying outcomes so listeners can inspect pre-state.
	// ChoiceIndex is the original node-local choice index used by the UI.
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

void UNPCDialogueComponent::AdvanceDialogue(ACharacter* Interactor)
{
	if (!bInConversation || !DialogueNodes.IsValidIndex(CurrentNodeIndex))
	{
		return;
	}

	const FDialogueNode& Node = DialogueNodes[CurrentNodeIndex];
	if (GetAvailableChoices().Num() > 0)
	{
		return;
	}

	if (Node.AutoNextNodeIndex < 0)
	{
		EndDialogue();
		return;
	}

	NavigateToNode(Node.AutoNextNodeIndex);
}

void UNPCDialogueComponent::EndDialogue()
{
	if (!bInConversation)
	{
		return;
	}

	ABlasterPlayerController* BlasterPC = ActiveInteractor.IsValid()
		? Cast<ABlasterPlayerController>(ActiveInteractor->GetController())
		: nullptr;

	bInConversation = false;
	CurrentNodeIndex = -1;
	ActiveInteractor = nullptr;
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}

	if (BlasterPC)
	{
		BlasterPC->ClientCloseNPCDialogue(GetOwner());
	}

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
		if (!DoesChoicePassRequirements(Choice, ActiveInteractor.Get()))
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

	case EDialogueOutcomeType::AdjustPersona:
		if (ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(Interactor))
		{
			if (UPersonaComponent* PersonaComponent = BlasterCharacter->GetPersonaComponent())
			{
				PersonaComponent->SetPersonaValue(PersonaComponent->GetPersonaValue() + Outcome.FloatValue);
			}
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
		if (Outcome.OutcomeType == EDialogueOutcomeType::TakeItem && Interactor)
		{
			ConsumeInteractorItemTag(Interactor, Outcome.TagPayload);
		}
		// Remaining item spawning/trading is handled by the owning NPC actor listening to OnDialogueChoiceSelected
		break;

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

int32 UNPCDialogueComponent::GetWorldFlagValue(const FGameplayTag& Flag) const
{
	UEscapeProgressComponent* EscapeProgress = GetEscapeProgress();
	if (!EscapeProgress || !Flag.IsValid())
	{
		return 0;
	}

	return EscapeProgress->GetWorldFlag(Flag);
}

float UNPCDialogueComponent::GetInteractorPersonaValue(ACharacter* Interactor) const
{
	const ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(Interactor);
	const UPersonaComponent* PersonaComponent = BlasterCharacter ? BlasterCharacter->GetPersonaComponent() : nullptr;
	return PersonaComponent ? PersonaComponent->GetPersonaValue() : 0.0f;
}

bool UNPCDialogueComponent::DoesInteractorHaveItemTag(ACharacter* Interactor, const FGameplayTag& ItemTag, bool bSpecialItemOnly) const
{
	if (!ItemTag.IsValid())
	{
		return false;
	}

	const ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(Interactor);
	if (!BlasterCharacter)
	{
		return false;
	}

	const auto DoesParcelMatch = [&ItemTag, bSpecialItemOnly](const AParcelActor* Parcel)
	{
		if (!IsValid(Parcel))
		{
			return false;
		}

		if (bSpecialItemOnly)
		{
			return Parcel->GetSpecialItemTags().HasTag(ItemTag);
		}

		return Parcel->GetItemData().ItemId == ItemTag
			|| Parcel->GetParcelTags().HasTag(ItemTag)
			|| Parcel->GetSpecialItemTags().HasTag(ItemTag);
	};

	if (const UInteractionComponent* InteractionComponent = BlasterCharacter->GetInteractionComponent())
	{
		if (DoesParcelMatch(InteractionComponent->GetCarriedParcel()))
		{
			return true;
		}
	}

	if (const UPlayerInventoryComponent* InventoryComponent = BlasterCharacter->GetPlayerInventoryComponent())
	{
		if (bSpecialItemOnly)
		{
			return InventoryComponent->HasSpecialItemTag(ItemTag);
		}

		return InventoryComponent->HasItemTag(ItemTag);
	}

	return false;
}

bool UNPCDialogueComponent::DoesChoiceRequirementPass(const FDialogueChoiceRequirement& Requirement, ACharacter* Interactor) const
{
	switch (Requirement.RequirementType)
	{
	case EDialogueChoiceRequirementType::None:
		return true;

	case EDialogueChoiceRequirementType::WorldFlagAtLeast:
		return Requirement.Tag.IsValid() && GetWorldFlagValue(Requirement.Tag) >= FMath::RoundToInt(FMath::Max(Requirement.ThresholdValue, 1.0f));

	case EDialogueChoiceRequirementType::WorldFlagAtMost:
		return Requirement.Tag.IsValid() && GetWorldFlagValue(Requirement.Tag) <= FMath::RoundToInt(Requirement.ThresholdValue);

	case EDialogueChoiceRequirementType::PersonaAtLeast:
		return GetInteractorPersonaValue(Interactor) >= Requirement.ThresholdValue;

	case EDialogueChoiceRequirementType::PersonaAtMost:
		return GetInteractorPersonaValue(Interactor) <= Requirement.ThresholdValue;

	case EDialogueChoiceRequirementType::HasItemTag:
		return DoesInteractorHaveItemTag(Interactor, Requirement.Tag, false);

	case EDialogueChoiceRequirementType::MissingItemTag:
		return !DoesInteractorHaveItemTag(Interactor, Requirement.Tag, false);

	case EDialogueChoiceRequirementType::HasSpecialItemTag:
		return DoesInteractorHaveItemTag(Interactor, Requirement.Tag, true);

	case EDialogueChoiceRequirementType::MissingSpecialItemTag:
		return !DoesInteractorHaveItemTag(Interactor, Requirement.Tag, true);

	default:
		return false;
	}
}

bool UNPCDialogueComponent::DoesChoicePassRequirements(const FDialogueChoice& Choice, ACharacter* Interactor) const
{
	if (Choice.RequiredWorldFlag.IsValid() && !IsWorldFlagSet(Choice.RequiredWorldFlag))
	{
		return false;
	}

	if (Choice.BlockingWorldFlag.IsValid() && IsWorldFlagSet(Choice.BlockingWorldFlag))
	{
		return false;
	}

	for (const FDialogueChoiceRequirement& Requirement : Choice.UnlockRequirements)
	{
		if (!DoesChoiceRequirementPass(Requirement, Interactor))
		{
			return false;
		}
	}

	return true;
}

bool UNPCDialogueComponent::BuildChoiceUIData(const FDialogueChoice& Choice, int32 ChoiceIndex, ACharacter* Interactor, FDialogueChoiceUIData& OutChoiceUIData) const
{
	OutChoiceUIData = FDialogueChoiceUIData();
	OutChoiceUIData.ChoiceIndex = ChoiceIndex;
	OutChoiceUIData.ChoiceText = Choice.ChoiceText;
	OutChoiceUIData.bIsEnabled = DoesChoicePassRequirements(Choice, Interactor);

	for (const FDialogueChoiceRequirement& Requirement : Choice.UnlockRequirements)
	{
		const bool bRequirementMet = DoesChoiceRequirementPass(Requirement, Interactor);
		if (!DoesRequirementHaveIndicator(Requirement))
		{
			continue;
		}

		if (bRequirementMet && !Requirement.bShowIndicatorWhenMet)
		{
			continue;
		}

		FDialogueChoiceIndicatorUIData IndicatorUIData;
		IndicatorUIData.IndicatorType = Requirement.IndicatorType;
		IndicatorUIData.IndicatorText = BuildRequirementIndicatorText(Requirement);
		IndicatorUIData.RelatedTag = Requirement.Tag;
		IndicatorUIData.IndicatorIcon = Requirement.IndicatorIcon;
		IndicatorUIData.bIsSatisfied = bRequirementMet;
		OutChoiceUIData.UnlockIndicators.Add(IndicatorUIData);
	}

	return true;
}

bool UNPCDialogueComponent::DoesRequirementHaveIndicator(const FDialogueChoiceRequirement& Requirement) const
{
	return Requirement.IndicatorType != EDialogueChoiceIndicatorType::None
		|| !Requirement.IndicatorText.IsEmpty()
		|| Requirement.IndicatorIcon != nullptr;
}

FText UNPCDialogueComponent::BuildRequirementIndicatorText(const FDialogueChoiceRequirement& Requirement) const
{
	if (!Requirement.IndicatorText.IsEmpty())
	{
		return Requirement.IndicatorText;
	}

	switch (Requirement.RequirementType)
	{
	case EDialogueChoiceRequirementType::PersonaAtLeast:
		return FText::Format(NSLOCTEXT("NPCDialogue", "PersonaAtLeastIndicator", "> {0}"), FText::AsNumber(FMath::RoundToInt(Requirement.ThresholdValue)));

	case EDialogueChoiceRequirementType::PersonaAtMost:
		return FText::Format(NSLOCTEXT("NPCDialogue", "PersonaAtMostIndicator", "< {0}"), FText::AsNumber(FMath::RoundToInt(Requirement.ThresholdValue)));

	case EDialogueChoiceRequirementType::WorldFlagAtLeast:
	case EDialogueChoiceRequirementType::WorldFlagAtMost:
	case EDialogueChoiceRequirementType::HasItemTag:
	case EDialogueChoiceRequirementType::MissingItemTag:
	case EDialogueChoiceRequirementType::HasSpecialItemTag:
	case EDialogueChoiceRequirementType::MissingSpecialItemTag:
		return GetGameplayTagDisplayText(Requirement.Tag);

	default:
		return FText::GetEmpty();
	}
}

FText UNPCDialogueComponent::GetGameplayTagDisplayText(const FGameplayTag& Tag) const
{
	if (!Tag.IsValid())
	{
		return FText::GetEmpty();
	}

	FString TagString = Tag.ToString();
	int32 SplitIndex = INDEX_NONE;
	if (TagString.FindLastChar(TEXT('.'), SplitIndex) && SplitIndex + 1 < TagString.Len())
	{
		TagString.RightChopInline(SplitIndex + 1);
	}

	return FText::FromString(TagString);
}

bool UNPCDialogueComponent::ConsumeInteractorItemTag(ACharacter* Interactor, const FGameplayTag& ItemTag) const
{
	if (!ItemTag.IsValid())
	{
		return false;
	}

	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(Interactor);
	if (!BlasterCharacter)
	{
		return false;
	}

	if (UInteractionComponent* InteractionComponent = BlasterCharacter->GetInteractionComponent())
	{
		if (AParcelActor* CarriedParcel = InteractionComponent->GetCarriedParcel())
		{
			const bool bMatchesTag = CarriedParcel->GetItemData().ItemId == ItemTag
				|| CarriedParcel->GetParcelTags().HasTag(ItemTag)
				|| CarriedParcel->GetSpecialItemTags().HasTag(ItemTag);
			if (bMatchesTag)
			{
				InteractionComponent->SetCarriedParcel(nullptr);
				CarriedParcel->Destroy();
				return true;
			}
		}
	}

	if (UPlayerInventoryComponent* InventoryComponent = BlasterCharacter->GetPlayerInventoryComponent())
	{
		for (AParcelActor* Item : InventoryComponent->GetCollectedItems())
		{
			if (!IsValid(Item))
			{
				continue;
			}

			const bool bMatchesTag = Item->GetItemData().ItemId == ItemTag
				|| Item->GetParcelTags().HasTag(ItemTag)
				|| Item->GetSpecialItemTags().HasTag(ItemTag);
			if (!bMatchesTag)
			{
				continue;
			}

			InventoryComponent->RemoveItem(Item);
			Item->Destroy();
			return true;
		}
	}

	return false;
}

void UNPCDialogueComponent::PushDialogueStateToInteractor(ACharacter* Interactor)
{
	if (!Interactor)
	{
		return;
	}

	ABlasterPlayerController* BlasterPC = Cast<ABlasterPlayerController>(Interactor->GetController());
	if (!BlasterPC)
	{
		return;
	}

	FDialogueUIState DialogueState;
	if (BuildDialogueUIState(DialogueState))
	{
		BlasterPC->ClientShowNPCDialogue(GetOwner(), DialogueState);
	}
}
