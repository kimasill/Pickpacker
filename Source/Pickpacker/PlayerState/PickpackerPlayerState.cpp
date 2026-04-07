// Fill out your copyright notice in the Description page of Project Settings.

#include "PickpackerPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "GameState/PickpackerGameState.h"
#include "GameMode/PickpackerGameMode.h"

APickpackerPlayerState::APickpackerPlayerState()
{
	bPCGReady = false;
	bHasReportedPCGReady = false;
}

void APickpackerPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APickpackerPlayerState, bPCGReady);
}

void APickpackerPlayerState::ServerReportPCGReady_Implementation()
{
	UE_LOG(LogTemp, Log, TEXT("[PickpackerPlayerState] ServerReportPCGReady called - Player: %s (NetMode=%d)"), *GetPlayerName(), GetWorld() ? (int32)GetWorld()->GetNetMode() : -1);

	if (bHasReportedPCGReady)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[PickpackerPlayerState] Already reported ready - Player: %s"), *GetPlayerName());
		return;
	}

	bPCGReady = true;
	bHasReportedPCGReady = true;

	if (UWorld* World = GetWorld())
	{
		// Prefer GameMode server tracker to avoid authority timing issues on GameState
		if (APickpackerGameMode* GM = World->GetAuthGameMode<APickpackerGameMode>())
		{
			GM->RegisterClientPCGReady(this);
		}

		// Also notify GameState (server-only path guarded inside)
		if (APickpackerGameState* GameState = World->GetGameState<APickpackerGameState>())
		{
			// Optional: expose a method if needed later
		}
	}
}