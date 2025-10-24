// Copyright notice

#include "PickpackerGameState.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Blaster/Subsystem/PCGDungeonSubSystem.h"
#include "GameFramework/GameModeBase.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Blaster/GameMode/PickpackerGameMode.h"

APickpackerGameState::APickpackerGameState()
{
	PrimaryActorTick.bCanEverTick = false;
	SeedSet = FSeedSet();
}

void APickpackerGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APickpackerGameState, SeedSet);
	DOREPLIFETIME(APickpackerGameState, bDungeonGenerated);
	DOREPLIFETIME(APickpackerGameState, PCGRunCounter);
	DOREPLIFETIME(APickpackerGameState, ClientsPCGReadyCount);
}

void APickpackerGameState::BeginPlay()
{
	Super::BeginPlay();
	PCGDungeonSubSystem = GetWorld()->GetSubsystem<UPCGDungeonSubSystem>();
	bAllClientsPCGReadyTriggered = false;
	ReadyPlayers.Reset();
	ExpectedClientCountSnapshot = -1;
}

void APickpackerGameState::SetSeedSet(const FSeedSet& NewSeedSet)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PickpackerGameState] Only server can set seed set"));
		return;
	}
	SeedSet = NewSeedSet;
	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameState] Seed set updated - Seed: %d, MissionId: %s"), SeedSet.Seed, *SeedSet.MissionId);
}

void APickpackerGameState::OnRep_SeedSet()
{
	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameState] Seed set replicated to client - Seed: %d, MissionId: %s"), SeedSet.Seed, *SeedSet.MissionId);
}

void APickpackerGameState::OnRep_DungeonGenerated()
{
	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameState] Dungeon generation status replicated - Generated: %s"), bDungeonGenerated ? TEXT("True") : TEXT("False"));
}

void APickpackerGameState::OnRep_PCGRunCounter()
{
	// Clients: run local PCG now with the current replicated seed
	if (!HasAuthority())
	{
		if (!PCGDungeonSubSystem && GetWorld())
		{
			PCGDungeonSubSystem = GetWorld()->GetSubsystem<UPCGDungeonSubSystem>();
		}
		if (PCGDungeonSubSystem)
		{
			PCGDungeonSubSystem->GenerateDungeon(SeedSet);
			UE_LOG(LogTemp, Log, TEXT("[PickpackerGameState] Client PCG triggered by server (RunCounter=%d)"), PCGRunCounter);
		}
	}
}

void APickpackerGameState::TriggerClientPCGRun()
{
	if (!HasAuthority())
	{
		return;
	}
	// Take a snapshot of expected client count at the time we trigger
	ExpectedClientCountSnapshot = GetExpectedClientCount();
	++PCGRunCounter; // replicate bump -> clients run OnRep
	ClientsPCGReadyCount = 0; // reset for new round
	bAllClientsPCGReadyTriggered = false;
	ReadyPlayers.Reset();
	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameState] Triggering client PCG run (RunCounter=%d), ExpectedClientsSnapshot=%d"), PCGRunCounter, ExpectedClientCountSnapshot);

	// Inform GameMode so it can also track independently and force StartGameplay if needed
	if (APickpackerGameMode* GM = GetWorld()->GetAuthGameMode<APickpackerGameMode>())
	{
		GM->OnServerTriggerClientPCGRun(ExpectedClientCountSnapshot);
	}
}

int32 APickpackerGameState::GetExpectedClientCount() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	// Server-authoritative path: prefer NetDriver connection count for accuracy during early join
	if (HasAuthority())
	{
		if (UNetDriver* NetDriver = World->GetNetDriver())
		{
			// Number of client connections to this server (listen/dedicated)
			return NetDriver->ClientConnections.Num();
		}

		// Fallback to counting non-local PlayerControllers
		int32 RemoteCount = 0;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* PC = It->Get();
			if (PC && !PC->IsLocalController())
			{
				RemoteCount++;
			}
		}
		return RemoteCount;
	}

	// Clients don't know how many other clients are expected from server perspective
	return 0;
}

void APickpackerGameState::HandleClientPCGReadyFor(APlayerState* ReportingPS)
{
	// Ensure this only runs on the server
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PickpackerGameState] HandleClientPCGReadyFor called on non-authority, ignoring (NetMode=%d)"), GetWorld() ? (int32)GetWorld()->GetNetMode() : -1);
		return;
	}

	bool bCounted = false;
	if (IsValid(ReportingPS))
	{
		if (ReadyPlayers.Contains(ReportingPS))
		{
			UE_LOG(LogTemp, Verbose, TEXT("[PickpackerGameState] Client already reported: %s"), *ReportingPS->GetPlayerName());
		}
		else
		{
			ReadyPlayers.Add(ReportingPS);
			ClientsPCGReadyCount++;
			bCounted = true;
		}
	}
	else
	{
		// Unknown reporter; still increment but warn
		ClientsPCGReadyCount++;
		bCounted = true;
		UE_LOG(LogTemp, Warning, TEXT("[PickpackerGameState] Client PCG ready without PlayerState - counting anonymously"));
	}

	// Prefer snapshot when valid to avoid timing race returning 0
	const int32 Expected = (ExpectedClientCountSnapshot >= 0) ? ExpectedClientCountSnapshot : GetExpectedClientCount();
	if (bCounted)
	{
		UE_LOG(LogTemp, Log, TEXT("[PickpackerGameState] Ready %d/%d (Snapshot=%d, Current=%d)"), ClientsPCGReadyCount, Expected, ExpectedClientCountSnapshot, GetExpectedClientCount());
	}

	// Also forward to GameMode to track on server
	if (APickpackerGameMode* GM = GetWorld()->GetAuthGameMode<APickpackerGameMode>())
	{
		GM->RegisterClientPCGReady(ReportingPS);
	}

	if (!bAllClientsPCGReadyTriggered && (Expected == 0 || ClientsPCGReadyCount >= Expected))
	{
		bAllClientsPCGReadyTriggered = true;

		UWorld* World = GetWorld();
		if (!World)
		{
			UE_LOG(LogTemp, Error, TEXT("[PickpackerGameState] World is null while finalizing"));
			return;
		}

		// Finalize PCG on server
		if (UPCGDungeonSubSystem* Subsys = World->GetSubsystem<UPCGDungeonSubSystem>())
		{
			Subsys->ServerFinalizePCG();
		}

		OnAllClientsPCGReady.Broadcast();
	}
}

void APickpackerGameState::HandleClientPCGReady()
{
	HandleClientPCGReadyFor(nullptr);
}

UPCGDungeonSubSystem* APickpackerGameState::GetPCGDungeonSubSystem()
{
	if (!PCGDungeonSubSystem && GetWorld())
	{
		PCGDungeonSubSystem = GetWorld()->GetSubsystem<UPCGDungeonSubSystem>();
	}
	return PCGDungeonSubSystem;
}

