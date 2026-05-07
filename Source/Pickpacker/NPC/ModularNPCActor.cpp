// ModularNPCActor.cpp

#include "ModularNPCActor.h"
#include "AI/ModularNPCAIController.h"
#include "AI/PPBlackboardKeys.h"
#include "AI/PPAIControllerBase.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "Components/NPCDialogueComponent.h"
#include "Components/NPCCombatComponent.h"
#include "Components/NPCCombatActionComponent.h"
#include "Components/NPCDetectionComponent.h"
#include "Components/NPCModuleComponent.h"
#include "Components/NPCLootTradeComponent.h"
#include "Components/EscapeProgressComponent.h"
#include "AI/PPPatrolRouteComponent.h"
#include "AI/UPPSightPerceptionComponent.h"
#include "Components/WidgetComponent.h"
#include "DataAssets/DA_NPCData.h"
#include "GameState/PickpackerGameState.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "PlayerController/BlasterPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AModularNPCActor::AModularNPCActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = AModularNPCAIController::StaticClass();

	// Create modular components
	DialogueComponent = CreateDefaultSubobject<UNPCDialogueComponent>(TEXT("DialogueComponent"));
	CombatComponent = CreateDefaultSubobject<UNPCCombatComponent>(TEXT("CombatComponent"));
	CombatActionComponent = CreateDefaultSubobject<UNPCCombatActionComponent>(TEXT("CombatActionComponent"));
	SightPerceptionComponent = CreateDefaultSubobject<UPPSightPerceptionComponent>(TEXT("SightPerceptionComponent"));
	DetectionComponent = CreateDefaultSubobject<UNPCDetectionComponent>(TEXT("DetectionComponent"));
	PatrolRouteComponent = CreateDefaultSubobject<UPPPatrolRouteComponent>(TEXT("PatrolRouteComponent"));
	LootTradeComponent = CreateDefaultSubobject<UNPCLootTradeComponent>(TEXT("LootTradeComponent"));

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Block);
	}

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
	EnsureMovementModeInitialized();
	HomeLocation = GetActorLocation();
	RefreshModuleComponents();

	// Listen for dialogue outcomes
	if (DialogueComponent)
	{
		DialogueComponent->OnDialogueChoiceSelected.AddDynamic(this, &AModularNPCActor::OnDialogueChoiceMade);
		DialogueComponent->OnDialogueEnded.AddDynamic(this, &AModularNPCActor::OnDialogueEnded);
	}

	if (CombatComponent)
	{
		CombatComponent->OnTargetAcquired.AddDynamic(this, &AModularNPCActor::OnCombatTargetAcquired);
		CombatComponent->OnTargetLost.AddDynamic(this, &AModularNPCActor::OnCombatTargetLost);
		CombatComponent->OnDied.AddDynamic(this, &AModularNPCActor::OnCombatDied);
	}

	// Set initial module states
	UpdateModuleStates();
	NotifyModulesOwnerReady();

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

	if (HasAuthority())
	{
		SyncCombatBlackboard();
	}
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

	EnsureDialogueDataInitialized();

	// Priority: Dialogue first, then Trade
	if (DialogueComponent && DialogueComponent->DialogueNodes.Num() > 0 && Disposition != ENPCDisposition::Hostile)
	{
		StartDialogueForInteractor(Interactor);
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

	EnsureDialogueDataInitialized();
	return true;
}

FText AModularNPCActor::GetInteractText_Implementation()
{
	EnsureDialogueDataInitialized();

	if (DialogueComponent && DialogueComponent->DialogueNodes.Num() > 0 && Disposition != ENPCDisposition::Hostile)
	{
		return FText::Format(NSLOCTEXT("NPC", "TalkTo", "Talk to {0}"), DisplayName);
	}
	if (Disposition == ENPCDisposition::Friendly && LootTradeComponent && LootTradeComponent->TradeInventory.Num() > 0)
	{
		return FText::Format(NSLOCTEXT("NPC", "TradeWith", "Trade with {0}"), DisplayName);
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
	return false;
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
	NotifyModulesDispositionChanged(OldDisposition, NewDisposition);

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
	NotifyModulesRoleChanged(OldRole, NewRole);
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

	NotifyModulesNarrativeStateEvaluated();
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
	EnsureMovementModeInitialized();

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

bool AModularNPCActor::StartDialogueForInteractor(ACharacter* Interactor)
{
	if (!DialogueComponent || !Interactor || Disposition == ENPCDisposition::Missing || Disposition == ENPCDisposition::Hostile)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ModularNPCActor] StartDialogueForInteractor rejected for actor '%s'. DialogueComponent=%s Interactor=%s Disposition=%d"),
			*GetName(),
			DialogueComponent ? TEXT("true") : TEXT("false"),
			Interactor ? TEXT("true") : TEXT("false"),
			static_cast<int32>(Disposition));
		return false;
	}

	if (DialogueComponent->bInConversation)
	{
		if (ActiveDialogueInteractor.Get() == Interactor)
		{
			if (ABlasterPlayerController* BlasterPC = Cast<ABlasterPlayerController>(Interactor->GetController()))
			{
				FDialogueUIState DialogueState;
				if (BuildDialogueUIState(DialogueState))
				{
					BlasterPC->ClientShowNPCDialogue(this, DialogueState);
				}
			}
		}
		return ActiveDialogueInteractor.Get() == Interactor;
	}

	if (!DialogueComponent->StartDialogue(Interactor))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ModularNPCActor] StartDialogue failed for actor '%s'. Dialogue node count=%d"),
			*GetName(),
			DialogueComponent->DialogueNodes.Num());
		return false;
	}

	ActiveDialogueInteractor = Interactor;

	if (ABlasterPlayerController* BlasterPC = Cast<ABlasterPlayerController>(Interactor->GetController()))
	{
		FDialogueUIState DialogueState;
		if (BuildDialogueUIState(DialogueState))
		{
			BlasterPC->ClientShowNPCDialogue(this, DialogueState);
		}
	}

	return true;
}

bool AModularNPCActor::SelectDialogueChoiceForInteractor(ACharacter* Interactor, int32 ChoiceIndex)
{
	if (!DialogueComponent || !DialogueComponent->bInConversation || !Interactor || ActiveDialogueInteractor.Get() != Interactor)
	{
		return false;
	}

	DialogueComponent->SelectChoice(Interactor, ChoiceIndex);

	if (ABlasterPlayerController* BlasterPC = Cast<ABlasterPlayerController>(Interactor->GetController()))
	{
		if (DialogueComponent->bInConversation)
		{
			FDialogueUIState DialogueState;
			if (BuildDialogueUIState(DialogueState))
			{
				BlasterPC->ClientShowNPCDialogue(this, DialogueState);
			}
		}
	}

	return true;
}

bool AModularNPCActor::AdvanceDialogueForInteractor(ACharacter* Interactor)
{
	if (!DialogueComponent || !DialogueComponent->bInConversation || !Interactor || ActiveDialogueInteractor.Get() != Interactor)
	{
		return false;
	}

	DialogueComponent->AdvanceDialogue(Interactor);

	if (ABlasterPlayerController* BlasterPC = Cast<ABlasterPlayerController>(Interactor->GetController()))
	{
		if (DialogueComponent->bInConversation)
		{
			FDialogueUIState DialogueState;
			if (BuildDialogueUIState(DialogueState))
			{
				BlasterPC->ClientShowNPCDialogue(this, DialogueState);
			}
		}
	}

	return true;
}

void AModularNPCActor::EndDialogueForInteractor(ACharacter* Interactor)
{
	if (!DialogueComponent || !DialogueComponent->bInConversation)
	{
		return;
	}

	if (Interactor && ActiveDialogueInteractor.Get() != Interactor)
	{
		return;
	}

	DialogueComponent->EndDialogue();
}

bool AModularNPCActor::BuildDialogueUIState(FDialogueUIState& OutDialogueState) const
{
	if (!DialogueComponent || !DialogueComponent->bInConversation)
	{
		return false;
	}

	if (!DialogueComponent->BuildDialogueUIState(OutDialogueState))
	{
		return false;
	}

	if (!DisplayName.IsEmpty())
	{
		OutDialogueState.SpeakerName = DisplayName;
	}

	UE_LOG(LogTemp, Log, TEXT("[ModularNPCActor] BuildDialogueUIState actor='%s' speaker='%s' text='%s' choices=%d"),
		*GetName(),
		*OutDialogueState.SpeakerName.ToString(),
		*OutDialogueState.DialogueText.ToString(),
		OutDialogueState.Choices.Num());

	return true;
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

	NPCData->BuildResolvedProfile(CachedProfile);
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

	TArray<FDialogueNode> ResolvedDialogueNodes;
	NPCData->BuildResolvedDialogueNodes(GetEffectiveNPCId(), ResolvedDialogueNodes);
	if (ResolvedDialogueNodes.Num() > 0)
	{
		CachedProfile.DialogueNodes = MoveTemp(ResolvedDialogueNodes);
	}

	Disposition = CachedProfile.DefaultDisposition;
	NPCRole = CachedProfile.DefaultRole;

	if (GetMesh())
	{
		if (USkeletalMesh* MeshOverride = NPCData->GetResolvedMeshOverride().LoadSynchronous())
		{
			GetMesh()->SetSkeletalMesh(MeshOverride);
		}

		const TArray<TSoftObjectPtr<UMaterialInterface>> MaterialOverrides = NPCData->GetResolvedMaterialOverrides();
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialOverrides.Num(); ++MaterialIndex)
		{
			if (UMaterialInterface* MaterialOverride = MaterialOverrides[MaterialIndex].LoadSynchronous())
			{
				GetMesh()->SetMaterial(MaterialIndex, MaterialOverride);
			}
		}

		if (UClass* AnimClass = NPCData->GetResolvedAnimClassOverride().LoadSynchronous())
		{
			if (AnimClass->IsChildOf(UAnimInstance::StaticClass()))
			{
				GetMesh()->SetAnimInstanceClass(AnimClass);
			}
		}
	}

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

	if (CombatComponent)
	{
		CombatComponent->ApplyCombatSettings(CachedProfile.CombatSettings);
	}
}

void AModularNPCActor::EnsureDialogueDataInitialized()
{
	if (!NPCData || !DialogueComponent || DialogueComponent->DialogueNodes.Num() > 0)
	{
		return;
	}

	if (!bProfileCached)
	{
		NPCData->BuildResolvedProfile(CachedProfile);
		bProfileCached = true;
	}

	if (NPCId.IsNone())
	{
		NPCId = CachedProfile.NPCId;
	}

	if (DisplayName.IsEmpty())
	{
		DisplayName = CachedProfile.DisplayName;
	}

	if (!CachedProfile.bEnableDialogue)
	{
		return;
	}

	TArray<FDialogueNode> ResolvedDialogueNodes;
	NPCData->BuildResolvedDialogueNodes(GetEffectiveNPCId(), ResolvedDialogueNodes);
	if (ResolvedDialogueNodes.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ModularNPCActor] Dialogue lookup returned no rows for NPC '%s' on actor '%s'. ConfigRowName/NPCId/DialogueScriptDataTable should match."),
			*GetEffectiveNPCId().ToString(),
			*GetName());
		return;
	}

	DialogueComponent->DialogueNodes = MoveTemp(ResolvedDialogueNodes);
	DialogueComponent->DefaultSpeakerName = CachedProfile.DisplayName;
	UE_LOG(LogTemp, Log, TEXT("[ModularNPCActor] Loaded %d dialogue nodes for NPC '%s' on actor '%s'."),
		DialogueComponent->DialogueNodes.Num(),
		*GetEffectiveNPCId().ToString(),
		*GetName());
}

void AModularNPCActor::UpdateModuleStates()
{
	RefreshModuleComponents();

	// Combat availability is separate from social disposition.
	// EngagementPolicy decides whether this NPC can initiate or only retaliate.
	const bool bCombatEnabled = bProfileCached ? CachedProfile.bEnableCombat : (Disposition == ENPCDisposition::Hostile);
	const bool bShouldCombat = bCombatEnabled && Disposition != ENPCDisposition::Missing;

	if (CombatComponent)
	{
		CombatComponent->SetCombatActive(bShouldCombat);
	}

	if (HasAuthority())
	{
		if (bShouldCombat)
		{
			StartCombatAI();
		}
		else if (ResolveAmbientBehaviorTree())
		{
			StartAmbientAI();
		}
		else
		{
			StopCombatAI();
		}
	}

	// Dialogue: available when not hostile and not missing
	// (component stays created but won't be interactable)

	// LootTrade: available when friendly
	// (component stays created but won't be interactable)

	NotifyModulesUpdated();
}

void AModularNPCActor::EnsureMovementModeInitialized()
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->Activate(true);

		if (Movement->MovementMode == MOVE_None)
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
	}
}

void AModularNPCActor::RefreshModuleComponents()
{
	TArray<UNPCModuleComponent*> Modules;
	GetComponents<UNPCModuleComponent>(Modules);

	RegisteredModules.Reset();
	for (UNPCModuleComponent* Module : Modules)
	{
		if (Module)
		{
			RegisteredModules.Add(Module);
		}
	}
}

void AModularNPCActor::NotifyModulesOwnerReady()
{
	for (UNPCModuleComponent* Module : RegisteredModules)
	{
		if (Module)
		{
			Module->HandleOwnerReady(this);
		}
	}
}

void AModularNPCActor::NotifyModulesDispositionChanged(ENPCDisposition OldDisposition, ENPCDisposition NewDisposition)
{
	for (UNPCModuleComponent* Module : RegisteredModules)
	{
		if (Module)
		{
			Module->HandleDispositionChanged(OldDisposition, NewDisposition);
		}
	}
}

void AModularNPCActor::NotifyModulesRoleChanged(ENPCRole OldRole, ENPCRole NewRole)
{
	for (UNPCModuleComponent* Module : RegisteredModules)
	{
		if (Module)
		{
			Module->HandleRoleChanged(OldRole, NewRole);
		}
	}
}

void AModularNPCActor::NotifyModulesNarrativeStateEvaluated()
{
	for (UNPCModuleComponent* Module : RegisteredModules)
	{
		if (Module)
		{
			Module->HandleNarrativeStateEvaluated();
		}
	}
}

void AModularNPCActor::NotifyModulesUpdated()
{
	for (UNPCModuleComponent* Module : RegisteredModules)
	{
		if (Module)
		{
			Module->HandleModulesUpdated();
		}
	}
}

void AModularNPCActor::StartCombatAI()
{
	if (!HasAuthority())
	{
		return;
	}

	AAIController* AIController = EnsureCombatAIController();
	if (!AIController)
	{
		return;
	}

	if (APPAIControllerBase* PPController = Cast<APPAIControllerBase>(AIController))
	{
		PPController->StopBehaviorTreeIfRunning();
	}

	if (UBehaviorTree* CombatBT = ResolveCombatBehaviorTree())
	{
		if (APPAIControllerBase* PPController = Cast<APPAIControllerBase>(AIController))
		{
			PPController->RunBehaviorTreeWithBlackboard(CombatBT, ResolveCombatBlackboard());
		}
	}
	else if (APPAIControllerBase* PPController = Cast<APPAIControllerBase>(AIController))
	{
		PPController->StopBehaviorTreeIfRunning();
	}

	SyncCombatBlackboard();

	if (CombatComponent && CombatComponent->CurrentTarget)
	{
		AIController->SetFocus(CombatComponent->CurrentTarget.Get());
	}
}

void AModularNPCActor::StopCombatAI()
{
	if (!HasAuthority())
	{
		return;
	}

	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		if (APPAIControllerBase* PPController = Cast<APPAIControllerBase>(AIController))
		{
			PPController->StopBehaviorTreeIfRunning();
		}

		AIController->StopMovement();
		AIController->ClearFocus(EAIFocusPriority::Gameplay);
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}

	ClearCombatBlackboard();
}

void AModularNPCActor::StartAmbientAI()
{
	if (!HasAuthority())
	{
		return;
	}

	AAIController* AIController = EnsureCombatAIController();
	if (!AIController)
	{
		return;
	}

	if (APPAIControllerBase* PPController = Cast<APPAIControllerBase>(AIController))
	{
		PPController->StopBehaviorTreeIfRunning();
	}

	if (UBehaviorTree* AmbientBT = ResolveAmbientBehaviorTree())
	{
		if (APPAIControllerBase* PPController = Cast<APPAIControllerBase>(AIController))
		{
			PPController->RunBehaviorTreeWithBlackboard(AmbientBT, ResolveAmbientBlackboard());
		}
	}
}

AAIController* AModularNPCActor::EnsureCombatAIController()
{
	if (!HasAuthority())
	{
		return nullptr;
	}

	if (AAIController* ExistingAI = Cast<AAIController>(GetController()))
	{
		return ExistingAI;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UClass* ControllerClass = AIControllerClass.Get();
	if (!ControllerClass)
	{
		ControllerClass = AModularNPCAIController::StaticClass();
	}

	if (!ControllerClass->IsChildOf(AAIController::StaticClass()))
	{
		return nullptr;
	}

	AAIController* SpawnedAI = World->SpawnActor<AAIController>(ControllerClass, GetActorLocation(), GetActorRotation());
	if (!SpawnedAI)
	{
		return nullptr;
	}

	SpawnedAI->Possess(this);
	return SpawnedAI;
}

void AModularNPCActor::SyncCombatBlackboard()
{
	AAIController* AIController = Cast<AAIController>(GetController());
	if (!AIController)
	{
		return;
	}

	UBlackboardComponent* BlackboardComp = AIController->FindComponentByClass<UBlackboardComponent>();
	if (!BlackboardComp)
	{
		return;
	}

	BlackboardComp->SetValueAsBool(PPBlackboardKeys::CombatActive, CombatComponent && CombatComponent->bCombatActive);
	const bool bCanInitiateEngagement = CombatComponent && CombatComponent->CanInitiateEngagement();
	BlackboardComp->SetValueAsBool(PPBlackboardKeys::CanInitiateEngagement, bCanInitiateEngagement);
	BlackboardComp->SetValueAsBool(PPBlackboardKeys::CanInitiateCombat, bCanInitiateEngagement);
	BlackboardComp->SetValueAsVector(PPBlackboardKeys::HomeLocation, HomeLocation);

	AActor* TargetActor = CombatComponent ? CombatComponent->CurrentTarget.Get() : nullptr;
	AActor* DetectedActor = DetectionComponent ? DetectionComponent->GetDetectedTarget() : nullptr;

	if (DetectionComponent)
	{
		BlackboardComp->SetValueAsEnum(PPBlackboardKeys::DetectionState, static_cast<uint8>(DetectionComponent->GetDetectionState()));
		BlackboardComp->SetValueAsFloat(PPBlackboardKeys::DetectionProgress, DetectionComponent->GetDetectionProgress01());
		BlackboardComp->SetValueAsBool(PPBlackboardKeys::DetectionSuspicious, DetectionComponent->IsSuspicious());
		BlackboardComp->SetValueAsBool(PPBlackboardKeys::DetectionConfirmed, DetectionComponent->IsConfirmed());

		if (DetectedActor)
		{
			BlackboardComp->SetValueAsObject(PPBlackboardKeys::DetectedPlayer, DetectedActor);
			BlackboardComp->SetValueAsVector(PPBlackboardKeys::LastKnownTargetLocation, DetectionComponent->GetLastKnownTargetLocation());
		}
		else
		{
			BlackboardComp->ClearValue(PPBlackboardKeys::DetectedPlayer);
			BlackboardComp->ClearValue(PPBlackboardKeys::LastKnownTargetLocation);
		}
	}

	if (TargetActor)
	{
		BlackboardComp->SetValueAsObject(PPBlackboardKeys::TargetActor, TargetActor);
		BlackboardComp->SetValueAsVector(PPBlackboardKeys::TargetLocation, TargetActor->GetActorLocation());
		BlackboardComp->SetValueAsBool(PPBlackboardKeys::CanAttackTarget, CombatComponent->IsActorInAttackRange(TargetActor));
	}
	else
	{
		BlackboardComp->ClearValue(PPBlackboardKeys::TargetActor);
		BlackboardComp->ClearValue(PPBlackboardKeys::TargetLocation);
		BlackboardComp->SetValueAsBool(PPBlackboardKeys::CanAttackTarget, false);
	}
}

void AModularNPCActor::ClearCombatBlackboard()
{
	AAIController* AIController = Cast<AAIController>(GetController());
	if (!AIController)
	{
		return;
	}

	UBlackboardComponent* BlackboardComp = AIController->FindComponentByClass<UBlackboardComponent>();
	if (!BlackboardComp)
	{
		return;
	}

	BlackboardComp->SetValueAsBool(PPBlackboardKeys::CombatActive, false);
	BlackboardComp->SetValueAsBool(PPBlackboardKeys::CanInitiateEngagement, false);
	BlackboardComp->SetValueAsBool(PPBlackboardKeys::CanInitiateCombat, false);
	BlackboardComp->SetValueAsBool(PPBlackboardKeys::DetectionSuspicious, false);
	BlackboardComp->SetValueAsBool(PPBlackboardKeys::DetectionConfirmed, false);
	BlackboardComp->SetValueAsFloat(PPBlackboardKeys::DetectionProgress, 0.0f);
	BlackboardComp->SetValueAsBool(PPBlackboardKeys::CanAttackTarget, false);
	BlackboardComp->ClearValue(PPBlackboardKeys::TargetActor);
	BlackboardComp->ClearValue(PPBlackboardKeys::DetectedPlayer);
	BlackboardComp->ClearValue(PPBlackboardKeys::TargetLocation);
	BlackboardComp->ClearValue(PPBlackboardKeys::LastKnownTargetLocation);
}

UBehaviorTree* AModularNPCActor::ResolveCombatBehaviorTree() const
{
	return NPCData ? NPCData->GetResolvedCombatBehaviorTree().LoadSynchronous() : nullptr;
}

UBlackboardData* AModularNPCActor::ResolveCombatBlackboard() const
{
	return NPCData ? NPCData->GetResolvedCombatBlackboard().LoadSynchronous() : nullptr;
}

UBehaviorTree* AModularNPCActor::ResolveAmbientBehaviorTree() const
{
	return NPCData ? NPCData->GetResolvedAmbientBehaviorTree().LoadSynchronous() : nullptr;
}

UBlackboardData* AModularNPCActor::ResolveAmbientBlackboard() const
{
	return NPCData ? NPCData->GetResolvedAmbientBlackboard().LoadSynchronous() : nullptr;
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

	if (!Node.Choices.IsValidIndex(ChoiceIndex))
	{
		return;
	}

	const FDialogueChoice& Choice = Node.Choices[ChoiceIndex];

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

void AModularNPCActor::OnDialogueEnded()
{
	ActiveDialogueInteractor = nullptr;
}

void AModularNPCActor::OnCombatTargetAcquired(AActor* NewTarget)
{
	if (!HasAuthority())
	{
		return;
	}

	StartCombatAI();

	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->SetFocus(NewTarget);
	}

	SyncCombatBlackboard();
}

void AModularNPCActor::OnCombatTargetLost()
{
	if (!HasAuthority())
	{
		return;
	}

	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->ClearFocus(EAIFocusPriority::Gameplay);
	}

	SyncCombatBlackboard();
}

void AModularNPCActor::OnCombatDied()
{
	if (!HasAuthority())
	{
		return;
	}

	StopCombatAI();
}
