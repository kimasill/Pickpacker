#include "Blaster/Components/CreditUnlockComponent.h"

#include "Net/UnrealNetwork.h"
#include "Blaster/GameMode/PickpackerGameMode.h"
#include "Blaster/GameState/PickpackerGameState.h"
#include "Engine/World.h"

UCreditUnlockComponent::UCreditUnlockComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UCreditUnlockComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCreditUnlockComponent, bUnlocked);
}

bool UCreditUnlockComponent::RequestUnlock(AActor* RequestingActor)
{
	if (bUnlocked)
	{
		return true;
	}

	if (!GetWorld())
	{
		return false;
	}

	APickpackerGameMode* GameMode = GetWorld()->GetAuthGameMode<APickpackerGameMode>();
	if (!GameMode)
	{
		return false;
	}

	APickpackerGameState* GameState = GameMode->GetGameState<APickpackerGameState>();

	if (UnlockCost > 0)
	{
		if (!GameState || GameState->GetTeamCredits() < UnlockCost)
		{
			return false;
		}

		const FString FinalReason = UnlockReason.IsEmpty()
			? FString::Printf(TEXT("Unlock %s"), *GetOwner()->GetName())
			: UnlockReason;

		GameMode->ApplyCreditDelta(-UnlockCost, FinalReason);
	}

	bUnlocked = true;
	OnUnlocked.Broadcast();
	return true;
}

void UCreditUnlockComponent::OnRep_Unlocked()
{
	if (bUnlocked)
	{
		OnUnlocked.Broadcast();
	}
}


