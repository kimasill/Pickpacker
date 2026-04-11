// PersonaComponent.cpp

#include "PersonaComponent.h"
#include "Components/EscapeProgressComponent.h"
#include "GameState/PickpackerGameState.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Net/UnrealNetwork.h"

UPersonaComponent::UPersonaComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	PersonaValue = InitialPersonaValue;
}

void UPersonaComponent::BeginPlay()
{
	Super::BeginPlay();

	PersonaValue = InitialPersonaValue;
	CachedTier = GetPersonaTier();

	// Initial sync
	if (HasAuthority())
	{
		SyncPersonaToWorldFlags();
	}

	UE_LOG(LogTemp, Log, TEXT("[PersonaComponent] Initialized. Value=%.1f Tier=%d"),
		PersonaValue, static_cast<int32>(CachedTier));
}

void UPersonaComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UPersonaComponent, PersonaValue);
}

// =========================================================================
// API
// =========================================================================

void UPersonaComponent::ApplyPersonaEvent(EPersonaEventType EventType, float CustomDelta)
{
	if (!HasAuthority())
	{
		return;
	}

	float Delta = (CustomDelta != 0.0f) ? CustomDelta : GetDefaultDeltaForEvent(EventType);
	if (FMath::IsNearlyZero(Delta))
	{
		return;
	}

	const float OldValue = PersonaValue;
	PersonaValue = FMath::Clamp(PersonaValue + Delta, 0.0f, MaxPersonaValue);

	if (!FMath::IsNearlyEqual(OldValue, PersonaValue))
	{
		const EPersonaTier OldTier = CachedTier;
		const EPersonaTier NewTier = GetPersonaTier();

		OnPersonaChanged.Broadcast(OldValue, PersonaValue, EventType);

		if (OldTier != NewTier)
		{
			CachedTier = NewTier;
			OnPersonaTierChanged.Broadcast(OldTier, NewTier);
		}

		SyncPersonaToWorldFlags();

		UE_LOG(LogTemp, Log, TEXT("[PersonaComponent] Event=%d Delta=%.1f Value=%.1f->%.1f Tier=%d"),
			static_cast<int32>(EventType), Delta, OldValue, PersonaValue, static_cast<int32>(NewTier));
	}
}

void UPersonaComponent::SetPersonaValue(float NewValue)
{
	if (!HasAuthority())
	{
		return;
	}

	const float OldValue = PersonaValue;
	PersonaValue = FMath::Clamp(NewValue, 0.0f, MaxPersonaValue);

	if (!FMath::IsNearlyEqual(OldValue, PersonaValue))
	{
		const EPersonaTier OldTier = CachedTier;
		const EPersonaTier NewTier = GetPersonaTier();

		OnPersonaChanged.Broadcast(OldValue, PersonaValue, EPersonaEventType::Custom);

		if (OldTier != NewTier)
		{
			CachedTier = NewTier;
			OnPersonaTierChanged.Broadcast(OldTier, NewTier);
		}

		SyncPersonaToWorldFlags();
	}
}

EPersonaTier UPersonaComponent::GetPersonaTier() const
{
	if (PersonaValue < 20.0f) return EPersonaTier::Hollow;
	if (PersonaValue < 40.0f) return EPersonaTier::Flickering;
	if (PersonaValue < 60.0f) return EPersonaTier::Emerging;
	if (PersonaValue < 80.0f) return EPersonaTier::Awakened;
	return EPersonaTier::Transcended;
}

float UPersonaComponent::GetDefaultDeltaForEvent(EPersonaEventType EventType) const
{
	switch (EventType)
	{
	case EPersonaEventType::KilledRobot:			return Delta_KilledRobot;
	case EPersonaEventType::DestroyedProperty:		return Delta_DestroyedProperty;
	case EPersonaEventType::DroppedItem:			return Delta_DroppedItem;
	case EPersonaEventType::BetrayedNPC:			return Delta_BetrayedNPC;
	case EPersonaEventType::PickedUpItem:			return Delta_PickedUpItem;
	case EPersonaEventType::HelpedNPC:				return Delta_HelpedNPC;
	case EPersonaEventType::CompletedQuest:			return Delta_CompletedQuest;
	case EPersonaEventType::GaveFoodToChef:			return Delta_GaveFoodToChef;
	case EPersonaEventType::PlayedMusicForJijibo:	return Delta_PlayedMusicForJijibo;
	case EPersonaEventType::GaveCultureToVictus:	return Delta_GaveCultureToVictus;
	case EPersonaEventType::FilledShelfForTick:		return Delta_FilledShelfForTick;
	case EPersonaEventType::TradedWithRusty:		return Delta_TradedWithRusty;
	default: return 0.0f;
	}
}

// =========================================================================
// Replication
// =========================================================================

void UPersonaComponent::OnRep_PersonaValue(float OldValue)
{
	const EPersonaTier OldTier = CachedTier;
	const EPersonaTier NewTier = GetPersonaTier();

	OnPersonaChanged.Broadcast(OldValue, PersonaValue, EPersonaEventType::None);

	if (OldTier != NewTier)
	{
		CachedTier = NewTier;
		OnPersonaTierChanged.Broadcast(OldTier, NewTier);
	}
}

// =========================================================================
// Internal
// =========================================================================

void UPersonaComponent::SyncPersonaToWorldFlags()
{
	if (!PersonaWorldFlagTag.IsValid())
	{
		return;
	}

	UEscapeProgressComponent* EscapeProgress = GetEscapeProgress();
	if (!EscapeProgress)
	{
		return;
	}

	// Store tier as integer world flag (0=Hollow ... 4=Transcended)
	const int32 TierValue = static_cast<int32>(GetPersonaTier());
	EscapeProgress->SetWorldFlag(PersonaWorldFlagTag, TierValue);
}

UEscapeProgressComponent* UPersonaComponent::GetEscapeProgress() const
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;

	APickpackerGameState* GS = Cast<APickpackerGameState>(World->GetGameState());
	if (!GS) return nullptr;

	return GS->GetEscapeProgressComponent();
}

bool UPersonaComponent::HasAuthority() const
{
	return GetOwner() && GetOwner()->HasAuthority();
}
