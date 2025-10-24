// Copyright notice

#include "PickpackerGameMode.h"
#include "PickpackerGameState.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/SpectatorPawn.h"
#include "Blaster/GameState/PickpackerGameState.h"
#include "Blaster/PlayerController/PickpackerPlayerController.h"
#include "Blaster/PlayerState/PickpackerPlayerState.h"

APickpackerGameMode::APickpackerGameMode()
{
	bStartPlayersAsSpectators = true;

	// Ensure our custom controller, game state, and player state are used
	PlayerControllerClass = APickpackerPlayerController::StaticClass();
	GameStateClass = APickpackerGameState::StaticClass();
	PlayerStateClass = APickpackerPlayerState::StaticClass();
}

void APickpackerGameMode::BeginPlay()
{
	Super::BeginPlay();

	PCGDungeonSubsystem = GetWorld()->GetSubsystem<UPCGDungeonSubSystem>();

	if (!PCGDungeonSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("[PickpackerGameMode] Failed to get PCG Dungeon Subsystem"));
	}
	else
	{
		PCGDungeonSubsystem->OnPCGGenerationComplete.AddDynamic(this, &APickpackerGameMode::OnPCGGenerationComplete);
	}

	if (APickpackerGameState* GS = Cast<APickpackerGameState>(GameState))
	{
		GS->OnAllClientsPCGReady.RemoveDynamic(this, &APickpackerGameMode::StartGameplay);
		GS->OnAllClientsPCGReady.AddDynamic(this, &APickpackerGameMode::StartGameplay);
	}
}

void APickpackerGameMode::OnMatchStateSet()
{
	Super::OnMatchStateSet();

	if (MatchState == MatchState::InProgress)
	{
		HandleMatchStart();
	}
}

void APickpackerGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// Hold spawn until PCG completes
	if (!bPCGReady || bPCGGenerationInProgress)
	{
		PendingSpawnControllers.Add(NewPlayer);
		return;
	}

	RestartPlayer(NewPlayer);
}

void APickpackerGameMode::CachePCGPlayerStarts()
{
	SelectedSpawnPoints.Reset();
	NextSpawnIndex = 0;
	if (!PCGDungeonSubsystem)
	{
		return;
	}
	TArray<AActor*> PlayerStarts;
	PCGDungeonSubsystem->GetPlayerSpawnPoints(PlayerStarts);
	for (AActor* A : PlayerStarts)
	{
		SelectedSpawnPoints.Add(A);
	}

	if (SelectedSpawnPoints.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Cached %d PCG PlayerSpawnPoints"), SelectedSpawnPoints.Num());
	}
}

void APickpackerGameMode::SelectRandomPlayerStarts()
{
	SelectedSpawnPoints.Reset();
	NextSpawnIndex = 0;

	TArray<AActor*> Starts;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), Starts);
	if (Starts.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PickpackerGameMode] No PlayerStart actors found"));
		return;
	}

	int32 FirstIdx = FMath::RandRange(0, Starts.Num() - 1);
	int32 SecondIdx = FirstIdx;
	if (Starts.Num() > 1)
	{
		while (SecondIdx == FirstIdx)
		{
			SecondIdx = FMath::RandRange(0, Starts.Num() - 1);
		}
	}

	SelectedSpawnPoints.Add(Starts[FirstIdx]);
	if (Starts.Num() > 1)
	{
		SelectedSpawnPoints.Add(Starts[SecondIdx]);
	}

	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Selected PlayerStarts: %s%s"),
		*Starts[FirstIdx]->GetName(),
		Starts.Num() > 1 ? *FString::Printf(TEXT(", %s"), *Starts[SecondIdx]->GetName()) : TEXT(""));
}

void APickpackerGameMode::OnPCGGenerationComplete()
{
	bPCGGenerationInProgress = false;
	bPCGReady = true;
	bGameplayStarted = false; // reset before next start

	// Prefer PCG-provided spawn points (actors)
	CachePCGPlayerStarts();
	if (SelectedSpawnPoints.Num() == 0)
	{
		SelectRandomPlayerStarts();
	}

	// Build transform list from PCG anchors if available
	CachePCGPlayerSpawnTransformsFromAnchors();

	// Do not spawn yet; wait for server finalize (triggered by GameState when all clients ready)
}

void APickpackerGameMode::CachePCGPlayerSpawnTransformsFromAnchors()
{
	SelectedSpawnTransforms.Reset();
	if (!PCGDungeonSubsystem)
	{
		return;
	}
	const FSeedSet SeedSet = PCGDungeonSubsystem->GetCurrentSeedSet();
	const TArray<FPCGAnchorData> Anchors = PCGDungeonSubsystem->GeneratePCGAnchors(SeedSet);
	for (const FPCGAnchorData& Anchor : Anchors)
	{
		if (Anchor.AnchorType == EPCGAnchorType::Extract)
		{
			SelectedSpawnTransforms.Add(FTransform(Anchor.Rotation, Anchor.Location));
		}
	}
	if (SelectedSpawnTransforms.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Cached %d PCG PlayerSpawn transforms from anchors"), SelectedSpawnTransforms.Num());
	}
}

void APickpackerGameMode::HandleMatchStart()
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PickpackerGameMode] Only server can handle match start"));
		return;
	}

	if (bPCGGenerationInProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PickpackerGameMode] PCG generation already in progress"));
		return;
	}

	if (!PCGDungeonSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("[PickpackerGameMode] PCG Dungeon Subsystem not available"));
		return;
	}

	APickpackerGameState* LocalGameState = Cast<APickpackerGameState>(GameState);
	if (!LocalGameState)
	{
		UE_LOG(LogTemp, Error, TEXT("[PickpackerGameMode] Pickpacker Game State not available"));
		return;
	}
	SetMissionConfig(CurrentMissionConfig.MissionId, CurrentMissionConfig.Seed);
	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Starting match with mission: %s, seed: %d"), 
		*CurrentMissionConfig.MissionId, CurrentMissionConfig.Seed);

	LocalGameState->SetSeedSet(CurrentMissionConfig);

	bPCGGenerationInProgress = true;
	bPCGReady = false;
	PCGDungeonSubsystem->GenerateDungeon(CurrentMissionConfig);
}

void APickpackerGameMode::SetMissionConfig(const FString& MissionId, int32 CustomSeed)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PickpackerGameMode] Only server can set mission config"));
		return;
	}

	CurrentMissionConfig.MissionId = MissionId;
	CurrentMissionConfig.Seed = CustomSeed > 0 ? CustomSeed : FMath::RandRange(1, 999999);

	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Mission config set - MissionId: %s, Seed: %d"), 
		*CurrentMissionConfig.MissionId, CurrentMissionConfig.Seed);
}

void APickpackerGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!bPCGReady)
	{
		if (APlayerController* PC = Cast<APlayerController>(NewPlayer))
		{
			PendingSpawnControllers.AddUnique(PC);
		}
		return;
	}

	// Prefer anchor-based transforms first
	if (SelectedSpawnTransforms.Num() > 0)
	{
		const FTransform SpawnTM = SelectedSpawnTransforms[NextTransformIndex % SelectedSpawnTransforms.Num()];
		NextTransformIndex++;
		APawn* NewPawn = SpawnDefaultPawnAtTransform(NewPlayer, SpawnTM);
		if (NewPawn)
		{
			NewPlayer->Possess(NewPawn);
			UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Spawned pawn at Anchor transform %s"), *SpawnTM.GetLocation().ToString());
			return;
		}
	}

	// Fallback to actor-based PCG points
	if (SelectedSpawnPoints.Num() == 0)
	{
		CachePCGPlayerStarts();
		if (SelectedSpawnPoints.Num() == 0)
		{
			SelectRandomPlayerStarts();
		}
	}

	AActor* ChosenStart = nullptr;
	if (SelectedSpawnPoints.Num() > 0)
	{
		ChosenStart = SelectedSpawnPoints[NextSpawnIndex % SelectedSpawnPoints.Num()].Get();
		NextSpawnIndex++;
	}

	if (ChosenStart)
	{
		const FTransform SpawnTM(ChosenStart->GetActorRotation(), ChosenStart->GetActorLocation());
		APawn* NewPawn = SpawnDefaultPawnAtTransform(NewPlayer, SpawnTM);
		if (NewPawn)
		{
			NewPlayer->Possess(NewPawn);
			UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Spawned pawn at PCG point %s"), *SpawnTM.GetLocation().ToString());
			return;
		}
		UE_LOG(LogTemp, Warning, TEXT("[PickpackerGameMode] Failed to spawn pawn at chosen PCG start; falling back"));
	}

	// Fallback to default behavior
	Super::RestartPlayer(NewPlayer);
}

void APickpackerGameMode::OnServerTriggerClientPCGRun(int32 ExpectedClients)
{
	if (!HasAuthority()) return;
	ExpectedClientCountSnapshot_Server = ExpectedClients;
	ClientsPCGReadyCount_Server = 0;
	bAllClientsPCGReadyTriggered_Server = false;
	ReadyPlayers_Server.Reset();
	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Server snapshot expected clients: %d"), ExpectedClientCountSnapshot_Server);
}

void APickpackerGameMode::RegisterClientPCGReady(APlayerState* ReportingPS)
{
	if (!HasAuthority()) return;
	bool bCounted = false;
	if (IsValid(ReportingPS))
	{
		if (!ReadyPlayers_Server.Contains(ReportingPS))
		{
			ReadyPlayers_Server.Add(ReportingPS);
			ClientsPCGReadyCount_Server++;
			bCounted = true;
		}
	}
	else
	{
		ClientsPCGReadyCount_Server++;
		bCounted = true;
	}

	const int32 Expected = ExpectedClientCountSnapshot_Server >= 0 ? ExpectedClientCountSnapshot_Server : 0;
	if (bCounted)
	{
		UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Server ready %d/%d"), ClientsPCGReadyCount_Server, Expected);
	}

	if (!bAllClientsPCGReadyTriggered_Server && (Expected == 0 || ClientsPCGReadyCount_Server >= Expected))
	{
		bAllClientsPCGReadyTriggered_Server = true;
		UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] All clients ready on server snapshot - starting gameplay"));
		StartGameplay();
	}
}

void APickpackerGameMode::StartGameplay()
{
	if (bGameplayStarted)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[PickpackerGameMode] StartGameplay() already executed - skipping"));
		return;
	}
	bGameplayStarted = true;

	APickpackerGameState* GS = Cast<APickpackerGameState>(GameState);
	if (GS)
	{
		UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] StartGameplay() called. GS ExpectedSnapshot=%d, GS Ready=%d, ServerSnapshot=%d, ServerReady=%d"),
			GS->GetExpectedClientCountSnapshot(), GS->GetClientsPCGReadyCount(), ExpectedClientCountSnapshot_Server, ClientsPCGReadyCount_Server);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[PickpackerGameMode] StartGameplay() called but GameState is null"));
	}

	bPCGReady = true;

	for (int32 i = PendingSpawnControllers.Num() - 1; i >= 0; --i)
	{
		if (APlayerController* PC = PendingSpawnControllers[i].Get())
		{
			RestartPlayer(PC);
		}
	}
	PendingSpawnControllers.Reset();

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;

		APawn* CurrentPawn = PC->GetPawn();
		const bool bNeedsSpawn = (CurrentPawn == nullptr) || CurrentPawn->IsA(ASpectatorPawn::StaticClass());
		if (bNeedsSpawn)
		{
			UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Forcing spawn for controller %s (spectating or no pawn)"), *PC->GetName());
			RestartPlayer(PC);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[PickpackerGameMode] Starting Pickpacker gameplay"));
}

