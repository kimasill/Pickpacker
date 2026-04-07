// ModularNPCActor.cpp

#include "ModularNPCActor.h"
#include "Components/NPCDialogueComponent.h"
#include "Components/NPCCombatComponent.h"
#include "Components/NPCLootTradeComponent.h"
#include "Components/EscapeProgressComponent.h"
#include "Components/WidgetComponent.h"
#include "DataAssets/DA_NPCData.h"
#include "GameState/PickpackerGameState.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AModularNPCActor::AModularNPCActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// Create modular components
	DialogueComponent = CreateDefaultSubobject<UNPCDialogueComponent>(TEXT("DialogueComponent"));
	CombatComponent = CreateDefaultSubobject<UNPCCombatComponent>(TEXT("CombatComponent"));
	LootTradeComponent = CreateDefaultSubobject<UNPCLootTradeComponent>(TEXT("LootTradeComponent"));

	// Name widget
	NameWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("NameWidget"));
	NameWidget->SetupAttachment(RootComponent);
	NameWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 110.0f));
	NameWidget->SetWidgetSpace(EWidgetSpace::Screen);
	NameWidget->SetDrawSize(FVector2D(200.0f, 40.0f));
}

void AModularNPCActor::BeginPlay()
{
	Super::BeginPlay();

	// Initialize from data asset
	InitializeFromData();

	// Listen for dialogue outcomes
	if (DialogueComponent)
	{
		DialogueComponent->OnDialogueChoiceSelected.AddDynamic(this, &AModularNPCActor::OnDialogueChoiceMade);
	}

	// Set initial module states
	UpdateModuleStates();

	// Start periodic narrative evaluation on server
	if (HasAuthority() && NarrativeEvalInterval > 0.0f)
	{
		GetWorldTimerManager().SetTimer(
			NarrativeEvalTimerHandle,
			this,
			&AModularNPCActor::EvaluateNarrativeState,
			NarrativeEvalInterval,
			true,
			1.0f // Initial delay to let the game state initialize
		);
	}
}

void AModularNPCActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AModularNPCActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AModularNPCActor, Disposition, COND_None);
	DOREPLIFETIME_CONDITION(AModularNPCActor, NPCRole, COND_None);
}

// =========================================================================
// IInteractableInterface
// =========================================================================

void AModularNPCActor::OnInteract_Implementation(ACharacter* Interactor)
{
	if (Disposition == ENPCDisposition::Missing)
	{
		return;
	}

	// Priority: Dialogue first, then Trade
	if (DialogueComponent && DialogueComponent->DialogueNodes.Num() > 0 && Disposition != ENPCDisposition::Hostile)
	{
		if (!DialogueComponent->bInConversation)
		{
			DialogueComponent->StartDialogue(Interactor);
		}
		return;
	}

	if (LootTradeComponent && LootTradeComponent->TradeInventory.Num() > 0 && Disposition == ENPCDisposition::Friendly)
	{
		if (!LootTradeComponent->bTradeOpen)
		{
			LootTradeComponent->OpenTrade(Interactor);
		}
		return;
	}
}

bool AModularNPCActor::CanInteract_Implementation(ACharacter* Interactor)
{
	if (Disposition == ENPCDisposition::Missing || Disposition == ENPCDisposition::Hostile)
	{
		return false;
	}

	if (!Interactor)
	{
		return false;
	}

	return true;
}

FText AModularNPCActor::GetInteractText_Implementation()
{
	if (Disposition == ENPCDisposition::Friendly && DialogueComponent && DialogueComponent->DialogueNodes.Num() > 0)
	{
		return FText::Format(NSLOCTEXT("NPC", "TalkTo", "Talk to {0}"), DisplayName);
	}
	if (Disposition == ENPCDisposition::Neutral)
	{
		return FText::Format(NSLOCTEXT("NPC", "Approach", "Approach {0}"), DisplayName);
	}
	return FText::GetEmpty();
}

void AModularNPCActor::GetInteractionUIData_Implementation(FInteractionUIData& OutData)
{
	OutData.InteractionType = EInteractionType::Talk;
	OutData.ActionText = GetInteractText_Implementation();
	OutData.DetailText = DisplayName;
}

void AModularNPCActor::StartHighlight_Implementation()
{
	// Override in Blueprint for highlight effects
}

void AModularNPCActor::EndHighlight_Implementation()
{
	// Override in Blueprint for highlight effects
}

bool AModularNPCActor::RequestShowInteractionUI_Implementation(ACharacter* Interactor)
{
	return CanInteract_Implementation(Interactor);
}

void AModularNPCActor::GetCreditUnlockInfo_Implementation(bool& bRequiresUnlock, int32& UnlockCost, FText& LockedMessage, FText& UnlockedMessage)
{
	bRequiresUnlock = false;
	UnlockCost = 0;
}

// =========================================================================
// API
// =========================================================================

void AModularNPCActor::SetDisposition(ENPCDisposition NewDisposition)
{
	if (Disposition == NewDisposition)
	{
		return;
	}

	const ENPCDisposition OldDisposition = Disposition;
	Disposition = NewDisposition;

	UpdateModuleStates();

	if (NewDisposition == ENPCDisposition::Missing)
	{
		Disappear();
	}
	else if (OldDisposition == ENPCDisposition::Missing)
	{
		Reappear();
	}

	OnDispositionChanged.Broadcast(OldDisposition, NewDisposition);
}

void AModularNPCActor::SetNPCRole(ENPCRole NewRole)
{
	if (NPCRole == NewRole)
	{
		return;
	}

	const ENPCRole OldRole = NPCRole;
	NPCRole = NewRole;
	OnRoleChanged.Broadcast(OldRole, NewRole);
}

void AModularNPCActor::EvaluateNarrativeState()
{
	if (!HasAuthority())
	{
		return;
	}

	if (!bProfileCached)
	{
		return;
	}

	UEscapeProgressComponent* EscapeProgress = GetEscapeProgress();
	if (!EscapeProgress)
	{
		return;
	}

	// Check disappear flag
	if (CachedProfile.DisappearFlag.IsValid())
	{
		if (EscapeProgress->GetWorldFlag(CachedProfile.DisappearFlag) != 0)
		{
			if (Disposition != ENPCDisposition::Missing)
			{
				SetDisposition(ENPCDisposition::Missing);
			}
			return;
		}
	}

	// Check hostile flag
	if (CachedProfile.HostileFlag.IsValid())
	{
		if (EscapeProgress->GetWorldFlag(CachedProfile.HostileFlag) != 0)
		{
			if (Disposition != ENPCDisposition::Hostile)
			{
				SetDisposition(ENPCDisposition::Hostile);
			}
			return;
		}
	}

	// If we were previously set to Missing or Hostile by flags that are no longer set, revert to default
	if (Disposition == ENPCDisposition::Missing || Disposition == ENPCDisposition::Hostile)
	{
		bool bShouldBeMissing = CachedProfile.DisappearFlag.IsValid() && EscapeProgress->GetWorldFlag(CachedProfile.DisappearFlag) != 0;
		bool bShouldBeHostile = CachedProfile.HostileFlag.IsValid() && EscapeProgress->GetWorldFlag(CachedProfile.HostileFlag) != 0;

		if (!bShouldBeMissing && !bShouldBeHostile)
		{
			SetDisposition(CachedProfile.DefaultDisposition);
		}
	}
}

void AModularNPCActor::Disappear()
{
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);

	if (CombatComponent)
	{
		CombatComponent->SetCombatActive(false);
	}
	if (DialogueComponent && DialogueComponent->bInConversation)
	{
		DialogueComponent->EndDialogue();
	}
	if (LootTradeComponent && LootTradeComponent->bTradeOpen)
	{
		LootTradeComponent->CloseTrade();
	}
}

void AModularNPCActor::Reappear()
{
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorTickEnabled(true);

	UpdateModuleStates();
}

FName AModularNPCActor::GetEffectiveNPCId() const
{
	if (!NPCId.IsNone())
	{
		return NPCId;
	}
	if (NPCData)
	{
		return NPCData->GetNPCId();
	}
	return NAME_None;
}

// =========================================================================
// Internal
// =========================================================================

void AModularNPCActor::OnRep_Disposition(ENPCDisposition OldDisposition)
{
	UpdateModuleStates();

	if (Disposition == ENPCDisposition::Missing)
	{
		SetActorHiddenInGame(true);
		SetActorEnableCollision(false);
	}
	else if (OldDisposition == ENPCDisposition::Missing)
	{
		SetActorHiddenInGame(false);
		SetActorEnableCollision(true);
	}

	OnDispositionChanged.Broadcast(OldDisposition, Disposition);
}

void AModularNPCActor::OnRep_Role(ENPCRole OldNPCRole)
{
	OnRoleChanged.Broadcast(OldNPCRole, NPCRole);
}

void AModularNPCActor::InitializeFromData()
{
	if (!NPCData)
	{
		return;
	}

	CachedProfile = NPCData->Profile;
	bProfileCached = true;

	// Set defaults from profile
	if (NPCId.IsNone())
	{
		NPCId = CachedProfile.NPCId;
	}

	if (DisplayName.IsEmpty())
	{
		DisplayName = CachedProfile.DisplayName;
	}

	Disposition = CachedProfile.DefaultDisposition;
	NPCRole = CachedProfile.DefaultRole;

	// Populate dialogue
	if (DialogueComponent && CachedProfile.bEnableDialogue)
	{
		DialogueComponent->DialogueNodes = CachedProfile.DialogueNodes;
		DialogueComponent->DefaultSpeakerName = CachedProfile.DisplayName;
	}

	// Populate trade
	if (LootTradeComponent && CachedProfile.bEnableLootTrade)
	{
		LootTradeComponent->TradeInventory = CachedProfile.TradeInventory;
	}
}

void AModularNPCActor::UpdateModuleStates()
{
	// Combat: active only when hostile
	if (CombatComponent)
	{
		const bool bShouldCombat = (Disposition == ENPCDisposition::Hostile) &&
			(bProfileCached ? CachedProfile.bEnableCombat : true);
		CombatComponent->SetCombatActive(bShouldCombat);
	}

	// Dialogue: available when not hostile and not missing
	// (component stays created but won't be interactable)

	// LootTrade: available when friendly
	// (component stays created but won't be interactable)
}

UEscapeProgressComponent* AModularNPCActor::GetEscapeProgress() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	APickpackerGameState* GS = Cast<APickpackerGameState>(World->GetGameState());
	if (!GS)
	{
		return nullptr;
	}

	return GS->GetEscapeProgressComponent();
}

void AModularNPCActor::OnDialogueChoiceMade(ACharacter* Interactor, int32 NodeIndex, int32 ChoiceIndex)
{
	if (!bProfileCached || !DialogueComponent)
	{
		return;
	}

	if (!DialogueComponent->DialogueNodes.IsValidIndex(NodeIndex))
	{
		return;
	}

	const FDialogueNode& Node = DialogueComponent->DialogueNodes[NodeIndex];

	// Get the actual available choices (filtered) and resolve the original
	TArray<FDialogueChoice> Available = DialogueComponent->GetAvailableChoices();
	// ChoiceIndex is for the available list
	// We need to search through all outcomes
	if (!Available.IsValidIndex(ChoiceIndex))
	{
		return;
	}

	const FDialogueChoice& Choice = Available[ChoiceIndex];

	for (const FDialogueOutcome& Outcome : Choice.Outcomes)
	{
		switch (Outcome.OutcomeType)
		{
		case EDialogueOutcomeType::ChangeDisposition:
			{
				const int32 DispositionValue = FMath::Clamp(Outcome.IntValue, 0, 3);
				SetDisposition(static_cast<ENPCDisposition>(DispositionValue));
			}
			break;

		case EDialogueOutcomeType::AttackPlayer:
			SetDisposition(ENPCDisposition::Hostile);
			if (CombatComponent)
			{
				CombatComponent->SetTarget(Interactor);
			}
			break;

		case EDialogueOutcomeType::OpenTrade:
			if (LootTradeComponent)
			{
				LootTradeComponent->OpenTrade(Interactor);
			}
			break;

		case EDialogueOutcomeType::Disappear:
			SetDisposition(ENPCDisposition::Missing);
			break;

		default:
			// Other outcomes handled by the component itself or Blueprint
			break;
		}
	}
}
