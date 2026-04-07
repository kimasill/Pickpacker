// Editor tool actor for setting team credits during PIE
#include "EditorTools/TeamCreditsEditorToolActor.h"
#include "GameState/PickpackerGameState.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#if WITH_EDITOR
#include "Editor.h"
#endif

ATeamCreditsEditorToolActor::ATeamCreditsEditorToolActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ATeamCreditsEditorToolActor::ApplyTeamCredits()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

#if WITH_EDITOR
	if (GEditor && (!World->IsGameWorld() || World->GetNetMode() == NM_Client))
	{
		UWorld* BestPIEWorld = nullptr;
		for (const FWorldContext& Context : GEditor->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World())
			{
				UWorld* CandidateWorld = Context.World();
				ENetMode NetMode = CandidateWorld->GetNetMode();
				if (NetMode == NM_ListenServer || NetMode == NM_DedicatedServer)
				{
					BestPIEWorld = CandidateWorld;
					break;
				}

				if (!BestPIEWorld)
				{
					BestPIEWorld = CandidateWorld;
				}
			}
		}

		if (BestPIEWorld)
		{
			World = BestPIEWorld;
		}
	}
#endif

	if (!World || !World->IsGameWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("[TeamCreditsEditorTool] Not in game world (PIE required)."));
		return;
	}

	APickpackerGameState* GameState = World->GetGameState<APickpackerGameState>();
	if (!GameState)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TeamCreditsEditorTool] PickpackerGameState not found."));
		return;
	}

	if (!GameState->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[TeamCreditsEditorTool] No authority to set team credits."));
		return;
	}

	const int32 NewCredits = FMath::Max(0, TargetTeamCredits);
	GameState->SetTeamCredits(NewCredits);

	UE_LOG(LogTemp, Log, TEXT("[TeamCreditsEditorTool] Team credits set to %d"), NewCredits);
}
