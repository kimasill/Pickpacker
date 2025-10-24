// Fill out your copyright notice in the Description page of Project Settings.

#include "PCGDungeonSubSystem.h"
#include "Blaster/PCG/PCGAnchorSystem.h"
#include "Blaster/PickpackerTypes/PickpackerTypes.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGSubsystem.h"
#include "PCGManagedResource.h"
#include "Components/PrimitiveComponent.h"
#include "Components/BoxComponent.h"
#include "PCG/PCGGenerator.h"
#include "Blaster/GameState/PickpackerGameState.h"
#include "Blaster/PlayerController/PickpackerPlayerController.h"
#include "Blaster/PlayerState/PickpackerPlayerState.h"
#include "Blaster/GameMode/PickpackerGameMode.h"

UPCGDungeonSubSystem::UPCGDungeonSubSystem()
{
	// Initialize default values
	PCGLevelName = TEXT("DungeonMap");
	PCGGraphName = TEXT("PCG_MultiFloorDungeon");
	bEnableDebugLogging = true;
	bValidateGeneration = true;
	GenerationTimeout = 30.0f;
	bIsGenerating = false;
	bGenerationComplete = false;
	GenerationStartTime = 0.0f;

	// Initialize anchor system (object; will set world later)
	AnchorSystem = CreateDefaultSubobject<UPCGAnchorSystem>(TEXT("AnchorSystem"));
}

void UPCGDungeonSubSystem::GenerateDungeon(const FSeedSet& SeedSet)
{
	if (!GetWorld())
	{
		return;
	}

	if (bIsGenerating)
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PCGDungeonSubSystem] PCG generation already in progress"));
		}
		return;
	}

	const bool bIsClient = (GetWorld()->GetNetMode() == NM_Client);
	bClientLocalPCG = bIsClient;

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] Starting PCG (seed=%d, mission=%s, %s)"), SeedSet.Seed, *SeedSet.MissionId, bIsClient ? TEXT("Client") : TEXT("Server"));
	}

	CurrentSeedSet = SeedSet;
	bIsGenerating = true;
	bGenerationComplete = false;
	GenerationStartTime = GetWorld()->GetTimeSeconds();

	// Start generation process
	InternalGenerateDungeon(SeedSet);
}

TArray<FPCGAnchorData> UPCGDungeonSubSystem::GeneratePCGAnchors(const FSeedSet& SeedSet)
{
	TArray<FPCGAnchorData> Anchors;

	// Clients may also build anchors locally for visualization; server will be the one to spawn gameplay actors

	// 시드 초기화 (결정적 결과)
	FMath::RandInit(SeedSet.Seed);

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] Generating anchors from %d cached spawn points"), CachedGeneratedActors.Num());
	}

	// 캐시된 스폰 포인트 액터들을 앵커로 변환
	int32 EnemySpawnIndex = 0;
	int32 ObjectiveSpawnIndex = 0;
	int32 HazardSpawnIndex = 0;

	for (const TWeakObjectPtr<AActor>& WeakActor : CachedGeneratedActors)
	{
		AActor* Actor = WeakActor.Get();
		if (!Actor || !IsValid(Actor))
		{
			continue;
		}

		FPCGAnchorData AnchorData;
		AnchorData.Location = Actor->GetActorLocation();
		AnchorData.Rotation = Actor->GetActorRotation();
		AnchorData.Metadata.Add(TEXT("MissionId"), SeedSet.MissionId);

		// PlayerSpawnPoint → Extract 앵커
		if (Actor->ActorHasTag(FName("PlayerSpawnPoint")))
		{
			AnchorData.AnchorType = EPCGAnchorType::Extract;
			AnchorData.Tag = FName("Extract_1");
			AnchorData.Metadata.Add(TEXT("SpawnType"), TEXT("Player"));
			AnchorData.Metadata.Add(TEXT("ExtractType"), TEXT("Primary"));
			Anchors.Add(AnchorData);
		}
		// EnemySpawnPoint → EnemySpawn 앵커
		else if (Actor->ActorHasTag(FName("EnemySpawnPoint")))
		{
			EnemySpawnIndex++;
			AnchorData.AnchorType = EPCGAnchorType::EnemySpawn;
			AnchorData.Tag = FName(*FString::Printf(TEXT("EnemySpawn_%d"), EnemySpawnIndex));
			AnchorData.Metadata.Add(TEXT("SpawnType"), TEXT("Enemy"));
			// 시드 기반으로 적 타입 할당 (결정적)
			if ((SeedSet.Seed + EnemySpawnIndex) % 2 == 0)
			{
				AnchorData.Metadata.Add(TEXT("EnemyType"), TEXT("Stalker"));
			}
			else
			{
				AnchorData.Metadata.Add(TEXT("EnemyType"), TEXT("Watcher"));
			}
			Anchors.Add(AnchorData);
		}
		// ObjectiveSpawnPoint → Objective 앵커
		else if (Actor->ActorHasTag(FName("ObjectiveSpawnPoint")))
		{
			ObjectiveSpawnIndex++;
			AnchorData.AnchorType = EPCGAnchorType::Objective;
			AnchorData.Tag = FName(*FString::Printf(TEXT("Objective_%d"), ObjectiveSpawnIndex));
			AnchorData.Metadata.Add(TEXT("SpawnType"), TEXT("Objective"));
			AnchorData.Metadata.Add(TEXT("Priority"), FString::FromInt(ObjectiveSpawnIndex));
			Anchors.Add(AnchorData);
		}
		// HazardSpawnPoint → HazardSpawn 앵커
		else if (Actor->ActorHasTag(FName("HazardSpawnPoint")))
		{
			HazardSpawnIndex++;
			AnchorData.AnchorType = EPCGAnchorType::HazardSpawn;
			AnchorData.Tag = FName(*FString::Printf(TEXT("HazardSpawn_%d"), HazardSpawnIndex));
			AnchorData.Metadata.Add(TEXT("SpawnType"), TEXT("Hazard"));
			AnchorData.Metadata.Add(TEXT("HazardType"), TEXT("ElectricFloor"));
			Anchors.Add(AnchorData);
		}
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] Generated %d anchors from spawn points"), Anchors.Num());
	}

	return Anchors;
}

bool UPCGDungeonSubSystem::ExecutePCGGraph(const FString& GraphName)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[PCGDungeonSubSystem] World is null"));
		return false;
	}

	const bool bIsServerAuth = IsPCGGenerationAllowed();
	const bool bShouldRunLocally = bIsServerAuth || bClientLocalPCG; // server always, client only when explicitly requested
	if (!bShouldRunLocally)
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Verbose, TEXT("[PCGDungeonSubSystem] Skipping PCG execution on this machine"));
		}
		return true;
	}

	TArray<AActor*> Generators;
	UGameplayStatics::GetAllActorsOfClass(World, APCGGenerator::StaticClass(), Generators);
	if (Generators.Num() == 0)
	{
		UGameplayStatics::GetAllActorsWithTag(World, FName("PCGGenerator"), Generators);
	}

	bool bDispatched = false;
	for (AActor* GenActor : Generators)
	{
		if (APCGGenerator* Gen = Cast<APCGGenerator>(GenActor))
		{
			if (bIsServerAuth)
			{
				Gen->RequestBlueprintPCG(CurrentSeedSet.Seed);
			}
			else
			{
				// Client: preview-only/local sim; BP should disable heavy spawners
				Gen->RequestBlueprintPCGWithOptions(CurrentSeedSet.Seed, /*bPreviewOnly*/ true);
			}
			bDispatched = true;
		}
	}

	if (bDispatched && bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] Dispatched PCG to %d generator(s) (%s)"), Generators.Num(), bIsServerAuth ? TEXT("Server") : TEXT("Client"));
	}
	return true;
}

void UPCGDungeonSubSystem::InternalGenerateDungeon(const FSeedSet& SeedSet)
{
	// Both server and client can execute the graph; only server will finalize/spawn
	FMath::RandInit(SeedSet.Seed);

	if (IsPCGGenerationAllowed())
	{
		if (!AnchorSystem)
		{
			AnchorSystem = NewObject<UPCGAnchorSystem>(this);
		}
		if (AnchorSystem)
		{
			AnchorSystem->Initialize(ObjectivesDataTable, SpawnersDataTable);
			AnchorSystem->SetWorldContext(GetWorld());
		}
		bAwaitingClientsForFinalize = true;
	}

	ExecutePCGGraph(PCGGraphName);
}

void UPCGDungeonSubSystem::SpawnPCGActors(const TArray<FPCGAnchorData>& Anchors)
{ 
	// No-op after refactor: PCG system spawns actors; subsystem consumes tags only.
}

bool UPCGDungeonSubSystem::ValidatePCGGeneration(const TArray<FPCGAnchorData>& Anchors) const
{
	if (!bValidateGeneration)
	{
		return true;
	}

	bool bValidationPassed = true;

	// Check if we have at least one objective
	bool bHasObjective = false;
	for (const FPCGAnchorData& Anchor : Anchors)
	{
		if (Anchor.AnchorType == EPCGAnchorType::Objective)
		{
			bHasObjective = true;
			break;
		}
	}

	if (!bHasObjective)
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PCGDungeonSubSystem] Validation failed - No objectives generated"));
		}
		bValidationPassed = false;
	}

	// Check if we have at least one extract
	bool bHasExtract = false;
	for (const FPCGAnchorData& Anchor : Anchors)
	{
		if (Anchor.AnchorType == EPCGAnchorType::Extract)
		{
			bHasExtract = true;
			break;
		}
	}

	if (!bHasExtract)
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PCGDungeonSubSystem] Validation failed - No extracts generated"));
		}
		bValidationPassed = false;
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] Validation %s"), bValidationPassed ? TEXT("PASSED") : TEXT("FAILED"));
	}

	return bValidationPassed;
}

void UPCGDungeonSubSystem::LogPCGGenerationResults(const TArray<FPCGAnchorData>& Anchors) const
{
	if (!bEnableDebugLogging)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] === PCG Generation Results ==="));
	UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] Total Anchors Generated: %d"), Anchors.Num());

	// Count anchors by type
	int32 ObjectiveCount = 0;
		int32 ExtractCount = 0;
		int32 EnemySpawnCount = 0;
		int32 HazardSpawnCount = 0;

	for (const FPCGAnchorData& Anchor : Anchors)
	{
		switch (Anchor.AnchorType)
		{
		case EPCGAnchorType::Objective:
			ObjectiveCount++;
			break;
		case EPCGAnchorType::Extract:
			ExtractCount++;
			break;
		case EPCGAnchorType::EnemySpawn:
			EnemySpawnCount++;
			break;
		case EPCGAnchorType::HazardSpawn:
			HazardSpawnCount++;
			break;
		default:
			break;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] Objectives: %d"), ObjectiveCount);
	UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] Extracts: %d"), ExtractCount);
	UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] Enemy Spawns: %d"), EnemySpawnCount);
	UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] Hazard Spawns: %d"), HazardSpawnCount);
	UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] ================================"));
}

void UPCGDungeonSubSystem::NotifyPCGGenerationComplete(UPCGComponent* InPCG)
{
	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] PCG generation completed - collecting spawn points"));
	}

	LastPCGComponent = InPCG;
	CachedGeneratedActors.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[PCGDungeonSubSystem] World is null"));
		return;
	}

	const bool bIsClientWorld = (World->GetNetMode() == NM_Client);

	if (!bIsClientWorld)
	{
		// 서버에서만 태그로 스폰 포인트를 수집
		TArray<AActor*> PlayerSpawnPoints;
		TArray<AActor*> EnemySpawnPoints;
		TArray<AActor*> ObjectiveSpawnPoints;
		TArray<AActor*> HazardSpawnPoints;

		UGameplayStatics::GetAllActorsWithTag(World, FName("PlayerSpawnPoint"), PlayerSpawnPoints);
		UGameplayStatics::GetAllActorsWithTag(World, FName("EnemySpawnPoint"), EnemySpawnPoints);
		UGameplayStatics::GetAllActorsWithTag(World, FName("ObjectiveSpawnPoint"), ObjectiveSpawnPoints);
		UGameplayStatics::GetAllActorsWithTag(World, FName("HazardSpawnPoint"), HazardSpawnPoints);

		// 서버만 캐시 유지 (Anchors/Gameplay processing only uses tags; player spawn now uses PlayerStart)
		CachedGeneratedActors.Append(PlayerSpawnPoints);
		CachedGeneratedActors.Append(EnemySpawnPoints);
		CachedGeneratedActors.Append(ObjectiveSpawnPoints);
		CachedGeneratedActors.Append(HazardSpawnPoints);

		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] Found spawn points - Player: %d, Enemy: %d, Objective: %d, Hazard: %d"),
				PlayerSpawnPoints.Num(), EnemySpawnPoints.Num(), ObjectiveSpawnPoints.Num(), HazardSpawnPoints.Num());
		}

		OnPCGGenerationComplete.Broadcast();

		// 클라이언트에게 PCG 생성 시작 신호
		if (APickpackerGameState* GS = World->GetGameState<APickpackerGameState>())
		{
			GS->TriggerClientPCGRun();

			// Start a failsafe finalize timer in case client RPCs never arrive
			World->GetTimerManager().ClearTimer(ForceFinalizeHandle);
			World->GetTimerManager().SetTimer(ForceFinalizeHandle, this, &UPCGDungeonSubSystem::ForceFinalizeAfterTimeout, 5.0f, false);

			const int32 ExpectedClients = GS->GetExpectedClientCount();
			if (ExpectedClients == 0)
			{
				if (bEnableDebugLogging)
				{
					UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] Expected clients = 0 right now; waiting briefly before finalizing"));
				}
				World->GetTimerManager().ClearTimer(FinalizeWaitHandle);
				World->GetTimerManager().SetTimer(FinalizeWaitHandle, this, &UPCGDungeonSubSystem::MaybeFinalizeAfterWait, 0.5f, false);
			}
			else
			{
				if (bEnableDebugLogging)
				{
					UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] Server PCG complete - waiting for %d clients"), ExpectedClients);
				}
			}
		}
	}
	else
	{
		// 클라이언트는 스폰 포인트 수집/스폰하지 않고 준비 완료만 보고
		OnClientPCGComplete.Broadcast();
		MarkClientPCGReady();

		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] Client PCG complete - reporting to server"));
		}
	}
}

void UPCGDungeonSubSystem::ForceFinalizeAfterTimeout()
{
	UWorld* World = GetWorld();
	if (!World || !IsPCGGenerationAllowed())
	{
		return;
	}
	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PCGDungeonSubSystem] Failsafe: forcing finalize after timeout"));
	}

	ServerFinalizePCG();
}

void UPCGDungeonSubSystem::MaybeFinalizeAfterWait()
{
	UWorld* World = GetWorld();
	if (!World || !IsPCGGenerationAllowed())
	{
		return;
	}
	if (APickpackerGameState* GS = World->GetGameState<APickpackerGameState>())
	{
		const int32 ExpectedClients = GS->GetExpectedClientCount();
		if (ExpectedClients == 0)
		{
			if (bEnableDebugLogging)
			{
				UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] Still no clients after wait - finalizing now"));
			}
			ServerFinalizePCG();
			GS->OnAllClientsPCGReady.Broadcast();
		}
		else if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] Client(s) detected after wait - waiting for readiness reports (%d)"), ExpectedClients);
		}
	}
}

void UPCGDungeonSubSystem::ServerFinalizePCG()
{
	if (!IsPCGGenerationAllowed())
	{
		UE_LOG(LogTemp, Warning, TEXT("[PCGDungeonSubSystem] ServerFinalizePCG called but not allowed (NetMode=%d)"), GetWorld() ? (int32)GetWorld()->GetNetMode() : -1);
		return;
	}
	if (!bAwaitingClientsForFinalize)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[PCGDungeonSubSystem] ServerFinalizePCG ignored - not awaiting clients"));
		// Still force gameplay start to unblock spawn, as PCG is already done
		if (APickpackerGameMode* GM = GetWorld()->GetAuthGameMode<APickpackerGameMode>())
		{
			GM->StartGameplay();
		}
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[PCGDungeonSubSystem] ServerFinalizePCG has no World"));
		return;
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] Server finalize - all clients ready, processing anchors"));
	}

	// 앵커 생성 (서버 전용)
	TArray<FPCGAnchorData> GameplayAnchors = GeneratePCGAnchors(CurrentSeedSet);
	
	// 앵커 처리 및 게임플레이 액터 스폰
	if (AnchorSystem && GameplayAnchors.Num() > 0)
	{
		AnchorSystem->ProcessAnchors(GameplayAnchors);
	}

	// 검증
	if (bValidateGeneration)
	{
		ValidatePCGGeneration(GameplayAnchors);
	}

	// 결과 로그
	LogPCGGenerationResults(GameplayAnchors);

	bIsGenerating = false;
	bGenerationComplete = true;
	bAwaitingClientsForFinalize = false;

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] Server finalize completed - gameplay spawn ready, players can spawn now"));
	}

	// Ensure gameplay actually starts even if delegate binding is missing
	if (APickpackerGameMode* GM = World->GetAuthGameMode<APickpackerGameMode>())
	{
		GM->StartGameplay();
	}
}

void UPCGDungeonSubSystem::MarkClientPCGReady()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() != NM_Client)
	{
		return;
	}

	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		// Prefer PlayerState RPC (more robust ownership on client)
		if (APlayerState* PS = PC->GetPlayerState<APlayerState>())
		{
			if(APickpackerPlayerController* PPC = Cast<APickpackerPlayerController>(PC))
			{
				PPC->ServerReportPCGReady();
				if (bEnableDebugLogging)
				{
					UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] Ready Client ID: %s (via PlayerState)"), *PPC->GetName());
				}
			}
		}

		// Fallback to PlayerController RPC
		if (APickpackerPlayerController* PPC = Cast<APickpackerPlayerController>(PC))
		{
			if (bEnableDebugLogging)
			{
				UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] Ready Client ID: %s"), *PPC->GetName());
			}
			PPC->ServerReportPCGReady();
			if (bEnableDebugLogging)
			{
				UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] Reported readiness via PlayerController RPC (fallback)"));
			}
		}
		else
		{
			// Retry shortly; controller class might not be swapped/possessed yet
			if (bEnableDebugLogging)
			{
				UE_LOG(LogTemp, Warning, TEXT("[PCGDungeonSubSystem] PlayerController is %s (not APickpackerPlayerController). Retrying..."), *PC->GetClass()->GetName());
			}
			World->GetTimerManager().ClearTimer(ClientReadyRetryHandle);
			World->GetTimerManager().SetTimer(ClientReadyRetryHandle, this, &UPCGDungeonSubSystem::TryReportClientReady, 0.5f, false);
		}
	}
	else
	{
		// Retry if no PC yet
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PCGDungeonSubSystem] No local PlayerController found; retrying..."));
		}
		World->GetTimerManager().ClearTimer(ClientReadyRetryHandle);
		World->GetTimerManager().SetTimer(ClientReadyRetryHandle, this, &UPCGDungeonSubSystem::TryReportClientReady, 0.5f, false);
	}
}

void UPCGDungeonSubSystem::TryReportClientReady()
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(World) || World->GetNetMode() != NM_Client)
	{
		return;
	}
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		// First try PlayerState RPC
		if (APlayerState* PS = PC->GetPlayerState<APlayerState>())
		{
			if (APickpackerPlayerState* PPS = Cast<APickpackerPlayerState>(PS))
			{
				PPS->ServerReportPCGReady();
				if (bEnableDebugLogging)
				{
					UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] Reported readiness via PlayerState RPC (retry)"));
				}
				World->GetTimerManager().ClearTimer(ClientReadyRetryHandle);
				return;
			}
		}

		// Then try PlayerController RPC
		if (APickpackerPlayerController* PPC = Cast<APickpackerPlayerController>(PC))
		{
			PPC->ServerReportPCGReady();
			if (bEnableDebugLogging)
			{
				UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] Reported readiness via PlayerController RPC (retry)"));
			}
			World->GetTimerManager().ClearTimer(ClientReadyRetryHandle);
			return;
		}

		// Keep retrying
		World->GetTimerManager().SetTimer(ClientReadyRetryHandle, this, &UPCGDungeonSubSystem::TryReportClientReady, 0.5f, false);
	}
}

bool UPCGDungeonSubSystem::IsPCGGenerationAllowed() const
{
	if (!GetWorld())
	{
		return false;
	}
	// Only allow gameplay spawning/processing on server. Clients still run local PCG elsewhere.
	return GetWorld()->GetNetMode() == NM_DedicatedServer || GetWorld()->GetNetMode() == NM_ListenServer;
}

FSeedSet UPCGDungeonSubSystem::GetCurrentSeedSet() const
{
	return CurrentSeedSet;
}

void UPCGDungeonSubSystem::SetDataTables(UDataTable* ObjectivesTable, UDataTable* SpawnersTable)
{
	ObjectivesDataTable = ObjectivesTable;
	SpawnersDataTable = SpawnersTable;

	if (AnchorSystem)
	{
		AnchorSystem->SetDataTables(ObjectivesTable, SpawnersTable);
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] Data tables set - Objectives: %s, Spawners: %s"),
			ObjectivesTable ? *ObjectivesTable->GetName() : TEXT("None"),
			SpawnersTable ? *SpawnersTable->GetName() : TEXT("None"));
	}
}

bool UPCGDungeonSubSystem::LoadPCGLevel(const FString& LevelName)
{
	// Level streaming/loading should remain server-authoritative
	if (!IsPCGGenerationAllowed())
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PCGDungeonSubSystem] Cannot load PCG level - not on server"));
		}
		return false;
	}

	// Load PCG level from plugin (stubbed)
	const FString PCGLevelPath = FString::Printf(TEXT("/PCGDungeonGenerator/PCGDungeonGenerator/Content/Maps/%s"), *LevelName);
	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] Loading PCG level: %s"), *PCGLevelPath);
	}
	// TODO: Implement actual level loading/streaming
	return true;
}

void UPCGDungeonSubSystem::GetSpawnPointsByTag(FName SpawnTag, TArray<AActor*>& OutActors) const
{
	OutActors.Reset();
	for (const TWeakObjectPtr<AActor>& WeakActor : CachedGeneratedActors)
	{
		if (AActor* A = WeakActor.Get())
		{
			if (A->ActorHasTag(SpawnTag))
			{
				OutActors.Add(A);
			}
		}
	}
}