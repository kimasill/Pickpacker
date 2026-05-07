#include "NPCAnimInstance.h"

#include "Animation/AnimMontage.h"
#include "Components/NPCCombatComponent.h"
#include "Components/NPCDetectionComponent.h"
#include "Components/NPCDialogueComponent.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/BlendSpace.h"
#include "DataAssets/DA_NPCData.h"
#include "DataAssets/NPCAnimationSet.h"
#include "NPC/ModularNPCActor.h"

bool UNPCAnimInstance::HasGroundedLocomotion() const
{
	return GroundedLocomotion != nullptr;
}

bool UNPCAnimInstance::ShouldPlayGroundedLocomotion() const
{
	return HasGroundedLocomotion() && Speed > LocomotionStartSpeed && !bIsInAir && !bIsDead && !bIsMissing;
}

UAnimSequenceBase* UNPCAnimInstance::GetPreferredIdleAsset() const
{
	if (bDetectionSuspicious && !bDetectionConfirmed && SuspiciousIdle)
	{
		return SuspiciousIdle;
	}

	if (bIsHostile && HostileIdle)
	{
		return HostileIdle;
	}

	if (bIsFriendly && FriendlyIdle)
	{
		return FriendlyIdle;
	}

	if (bIsNeutral && NeutralIdle)
	{
		return NeutralIdle;
	}

	return DefaultIdle;
}

UAnimSequenceBase* UNPCAnimInstance::GetSuspiciousIdleAsset() const
{
	return SuspiciousIdle.Get();
}

UAnimSequenceBase* UNPCAnimInstance::GetConversationAsset() const
{
	return (bInConversation && !bIsDead && !bIsMissing) ? ConversationLoop.Get() : nullptr;
}

UAnimSequenceBase* UNPCAnimInstance::GetFallAsset() const
{
	return (!bIsDead && !bIsMissing && bIsInAir) ? FallLoop.Get() : nullptr;
}

UAnimSequenceBase* UNPCAnimInstance::GetDeathAsset() const
{
	return bIsDead ? DeathPose.Get() : nullptr;
}

UAnimSequenceBase* UNPCAnimInstance::GetMissingAsset() const
{
	return bIsMissing ? MissingPose.Get() : nullptr;
}

UAnimMontage* UNPCAnimInstance::GetAttackMontageAsset() const
{
	return AttackMontage.Get();
}

UAnimMontage* UNPCAnimInstance::GetHitReactMontageAsset() const
{
	return HitReactMontage.Get();
}

UAnimMontage* UNPCAnimInstance::GetInteractionMontageAsset() const
{
	return InteractionMontage.Get();
}

void UNPCAnimInstance::CacheCharacterOwner()
{
	Super::CacheCharacterOwner();
	NPCOwner = Cast<AModularNPCActor>(GetCharacterOwner());
	CachedNPCData = NPCOwner ? NPCOwner->NPCData : nullptr;
	RefreshAnimationSet();
}

void UNPCAnimInstance::RefreshAnimationSet()
{
	AnimationSet = CachedNPCData ? CachedNPCData->GetResolvedAnimationSet().LoadSynchronous() : nullptr;

	if (AnimationSet)
	{
		GroundedLocomotion = AnimationSet->GroundedLocomotion.LoadSynchronous();
		DefaultIdle = AnimationSet->DefaultIdle.LoadSynchronous();
		FriendlyIdle = AnimationSet->FriendlyIdle.LoadSynchronous();
		NeutralIdle = AnimationSet->NeutralIdle.LoadSynchronous();
		HostileIdle = AnimationSet->HostileIdle.LoadSynchronous();
		SuspiciousIdle = AnimationSet->SuspiciousIdle.LoadSynchronous();
		ConversationLoop = AnimationSet->ConversationLoop.LoadSynchronous();
		MissingPose = AnimationSet->MissingPose.LoadSynchronous();
		DeathPose = AnimationSet->DeathPose.LoadSynchronous();
		FallLoop = AnimationSet->FallLoop.LoadSynchronous();
		AttackMontage = AnimationSet->AttackMontage.LoadSynchronous();
		HitReactMontage = AnimationSet->HitReactMontage.LoadSynchronous();
		InteractionMontage = AnimationSet->InteractionMontage.LoadSynchronous();
		return;
	}

	GroundedLocomotion = nullptr;
	DefaultIdle = nullptr;
	FriendlyIdle = nullptr;
	NeutralIdle = nullptr;
	HostileIdle = nullptr;
	SuspiciousIdle = nullptr;
	ConversationLoop = nullptr;
	MissingPose = nullptr;
	DeathPose = nullptr;
	FallLoop = nullptr;
	AttackMontage = nullptr;
	HitReactMontage = nullptr;
	InteractionMontage = nullptr;
}

void UNPCAnimInstance::UpdateCharacterSpecificData(float /*DeltaTime*/)
{
	if (NPCOwner == nullptr)
	{
		CacheCharacterOwner();
	}

	if (NPCOwner == nullptr)
	{
		return;
	}

	if (CachedNPCData != NPCOwner->NPCData)
	{
		CachedNPCData = NPCOwner->NPCData;
		RefreshAnimationSet();
	}

	CurrentDisposition = NPCOwner->Disposition;
	CurrentRole = NPCOwner->NPCRole;

	bIsFriendly = CurrentDisposition == ENPCDisposition::Friendly;
	bIsNeutral = CurrentDisposition == ENPCDisposition::Neutral;
	bIsHostile = CurrentDisposition == ENPCDisposition::Hostile;
	bIsMissing = CurrentDisposition == ENPCDisposition::Missing;

	if (const UNPCCombatComponent* CombatComponent = NPCOwner->CombatComponent)
	{
		bCombatActive = CombatComponent->bCombatActive;
		bIsDead = CombatComponent->bIsDead;
	}
	else
	{
		bCombatActive = false;
		bIsDead = false;
	}

	if (const UNPCDetectionComponent* DetectionComponent = NPCOwner->DetectionComponent)
	{
		bDetectionSuspicious = DetectionComponent->IsSuspicious();
		bDetectionConfirmed = DetectionComponent->IsConfirmed();
	}
	else
	{
		bDetectionSuspicious = false;
		bDetectionConfirmed = false;
	}

	if (const UNPCDialogueComponent* DialogueComponent = NPCOwner->DialogueComponent)
	{
		bInConversation = DialogueComponent->bInConversation;
	}
	else
	{
		bInConversation = false;
	}

	bHasMissingState = GetMissingAsset() != nullptr;
	bHasDeadState = GetDeathAsset() != nullptr;
	bHasAirState = GetFallAsset() != nullptr;
	bHasConversationState = GetConversationAsset() != nullptr;

	if (bHasMissingState)
	{
		CurrentTopLevelAnimState = ENPCTopLevelAnimState::Missing;
	}
	else if (bHasDeadState)
	{
		CurrentTopLevelAnimState = ENPCTopLevelAnimState::Dead;
	}
	else if (bHasAirState)
	{
		CurrentTopLevelAnimState = ENPCTopLevelAnimState::Air;
	}
	else if (bHasConversationState)
	{
		CurrentTopLevelAnimState = ENPCTopLevelAnimState::Conversation;
	}
	else
	{
		CurrentTopLevelAnimState = ENPCTopLevelAnimState::Grounded;
	}
}
