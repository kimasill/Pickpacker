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

	// Initialize anchor system
	AnchorSystem = CreateDefaultSubobject<UPCGAnchorSystem>(TEXT("AnchorSystem"));
}

void UPCGDungeonSubSystem::GenerateDungeon(const FSeedSet& SeedSet)
{
	if (!IsPCGGenerationAllowed())
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PCGDungeonSubSystem] PCG generation not allowed - not on server"));
		}
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

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] Starting PCG dungeon generation with seed: %d, mission: %s"),
			SeedSet.Seed, *SeedSet.MissionId);
	}

	CurrentSeedSet = SeedSet;
	bIsGenerating = true;
	bGenerationComplete = false;
	GenerationStartTime = GetWorld()->GetTimeSeconds();

	// Start generation process
	InternalGenerateDungeon(SeedSet);
}

bool UPCGDungeonSubSystem::IsPCGGenerationAllowed() const
{
	if (!GetWorld())
	{
		return false;
	}

	// Only allow PCG generation on server
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

TArray<FPCGAnchorData> UPCGDungeonSubSystem::GeneratePCGAnchors(const FSeedSet& SeedSet)
{
	TArray<FPCGAnchorData> GeneratedAnchors;

	if (!IsPCGGenerationAllowed())
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PCGDungeonSubSystem] Cannot generate PCG anchors - not on server"));
		}
		return GeneratedAnchors;
	}

	// Set random seed for deterministic generation
	FMath::RandInit(SeedSet.Seed);

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] Generating PCG anchors with seed: %d"), SeedSet.Seed);
	}

	// Generate objective anchors using PCG generated room centers
	int32 ObjectiveCount = FMath::RandRange(1, 3);
	for (int32 i = 0; i < ObjectiveCount; ++i)
	{
		FPCGAnchorData ObjectiveAnchor;
		ObjectiveAnchor.AnchorType = EPCGAnchorType::Objective;
		ObjectiveAnchor.Tag = FName(*FString::Printf(TEXT("Objective_%d"), i + 1));
		ObjectiveAnchor.Location = FindSuitableRoomCenter(); // Use PCG generated room center
		ObjectiveAnchor.Rotation = FRotator(0.0f, FMath::RandRange(0.0f, 360.0f), 0.0f);
		ObjectiveAnchor.Metadata.Add(TEXT("MissionId"), SeedSet.MissionId);
		ObjectiveAnchor.Metadata.Add(TEXT("Priority"), FString::FromInt(i + 1));
		ObjectiveAnchor.Metadata.Add(TEXT("RoomType"), TEXT("Large"));
		GeneratedAnchors.Add(ObjectiveAnchor);
	}

	// Generate extract anchors
	int32 ExtractCount = FMath::RandRange(1, 2);
	for (int32 i = 0; i < ExtractCount; ++i)
	{
		FPCGAnchorData ExtractAnchor;
		ExtractAnchor.AnchorType = EPCGAnchorType::Extract;
		ExtractAnchor.Tag = FName(*FString::Printf(TEXT("Extract_%d"), i + 1));
		ExtractAnchor.Location = FVector(
			FMath::RandRange(-800.0f, 800.0f),
			FMath::RandRange(-800.0f, 800.0f),
			FMath::RandRange(0.0f, 200.0f)
		);
		ExtractAnchor.Rotation = FRotator(0.0f, FMath::RandRange(0.0f, 360.0f), 0.0f);
		ExtractAnchor.Metadata.Add(TEXT("MissionId"), SeedSet.MissionId);
		ExtractAnchor.Metadata.Add(TEXT("ExtractType"), TEXT("Primary"));
		GeneratedAnchors.Add(ExtractAnchor);
	}

	// Generate enemy spawn anchors using PCG generated spawn points
	TArray<FVector> EnemySpawnPoints = FindEnemySpawnPoints();
	for (int32 i = 0; i < EnemySpawnPoints.Num(); ++i)
	{
		FPCGAnchorData EnemySpawnAnchor;
		EnemySpawnAnchor.AnchorType = EPCGAnchorType::EnemySpawn;
		EnemySpawnAnchor.Tag = FName(*FString::Printf(TEXT("EnemySpawn_%d"), i + 1));
		EnemySpawnAnchor.Location = EnemySpawnPoints[i]; // Use PCG generated spawn points
		EnemySpawnAnchor.Rotation = FRotator(0.0f, FMath::RandRange(0.0f, 360.0f), 0.0f);
		EnemySpawnAnchor.Metadata.Add(TEXT("MissionId"), SeedSet.MissionId);
		EnemySpawnAnchor.Metadata.Add(TEXT("EnemyType"), (i % 2 == 0) ? TEXT("Stalker") : TEXT("Watcher"));
		EnemySpawnAnchor.Metadata.Add(TEXT("SpawnArea"), TEXT("Corridor"));
		GeneratedAnchors.Add(EnemySpawnAnchor);
	}

	// Generate hazard spawn anchors using PCG generated hazard points
	TArray<FVector> HazardSpawnPoints = FindHazardSpawnPoints();
	for (int32 i = 0; i < HazardSpawnPoints.Num(); ++i)
	{
		FPCGAnchorData HazardSpawnAnchor;
		HazardSpawnAnchor.AnchorType = EPCGAnchorType::HazardSpawn;
		HazardSpawnAnchor.Tag = FName(*FString::Printf(TEXT("HazardSpawn_%d"), i + 1));
		HazardSpawnAnchor.Location = HazardSpawnPoints[i]; // Use PCG generated hazard points
		HazardSpawnAnchor.Rotation = FRotator(0.0f, FMath::RandRange(0.0f, 360.0f), 0.0f);
		HazardSpawnAnchor.Metadata.Add(TEXT("MissionId"), SeedSet.MissionId);
		HazardSpawnAnchor.Metadata.Add(TEXT("HazardType"), TEXT("ElectricFloor"));
		HazardSpawnAnchor.Metadata.Add(TEXT("FloorArea"), TEXT("Chokepoint"));
		GeneratedAnchors.Add(HazardSpawnAnchor);
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] Generated %d PCG anchors"), GeneratedAnchors.Num());
	}

	return GeneratedAnchors;
}

bool UPCGDungeonSubSystem::LoadPCGLevel(const FString& LevelName)
{
	if (!IsPCGGenerationAllowed())
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PCGDungeonSubSystem] Cannot load PCG level - not on server"));
		}
		return false;
	}

	// Load PCG level from plugin
	FString PCGLevelPath = FString::Printf(TEXT("/PCGDungeonGenerator/PCGDungeonGenerator/Content/Maps/%s"), *LevelName);
	
	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] Loading PCG level: %s"), *PCGLevelPath);
	}

	// TODO: Implement actual level loading logic
	// This would involve loading the PCG level and setting up the PCG component
	
	return true;
}

bool UPCGDungeonSubSystem::ExecutePCGGraph(const FString& GraphName)
{
	if (!IsPCGGenerationAllowed())
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PCGDungeonSubSystem] Cannot execute PCG graph - not on server"));
		}
		return false;
	}

	// Execute PCG graph from plugin
	FString PCGGraphPath = FString::Printf(TEXT("/PCGDungeonGenerator/PCGDungeonGenerator/Content/PCG/%s"), *GraphName);
	
	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] Executing PCG graph: %s"), *PCGGraphPath);
	}

	// TODO: Implement actual PCG graph execution
	// This would involve loading the PCG graph and executing it
	
	return true;
}

void UPCGDungeonSubSystem::InternalGenerateDungeon(const FSeedSet& SeedSet)
{
	if (!IsPCGGenerationAllowed())
	{
		return;
	}

	// Set random seed for deterministic generation
	FMath::RandInit(SeedSet.Seed);

	// Initialize anchor system
	if (!AnchorSystem)
	{
		AnchorSystem = NewObject<UPCGAnchorSystem>(this);
	}

	if (AnchorSystem)
	{
		AnchorSystem->Initialize(ObjectivesDataTable, SpawnersDataTable);
	}

	// Generate PCG anchors
	TArray<FPCGAnchorData> GeneratedAnchors = GeneratePCGAnchors(SeedSet);

	// Process anchors through anchor system
	if (AnchorSystem && GeneratedAnchors.Num() > 0)
	{
		AnchorSystem->ProcessAnchors(GeneratedAnchors);
	}

	// Spawn PCG actors
	SpawnPCGActors(GeneratedAnchors);

	// Validate generation if enabled
	if (bValidateGeneration)
	{
		ValidatePCGGeneration(GeneratedAnchors);
	}

	// Log results
	LogPCGGenerationResults(GeneratedAnchors);

	// Mark generation as complete
	bIsGenerating = false;
	bGenerationComplete = true;

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] PCG dungeon generation completed"));
	}
}

void UPCGDungeonSubSystem::SpawnPCGActors(const TArray<FPCGAnchorData>& Anchors)
{
	if (!IsPCGGenerationAllowed())
	{
		return;
	}

	for (const FPCGAnchorData& Anchor : Anchors)
	{
		// TODO: Implement actual actor spawning based on anchor data
		// This would involve:
		// 1. Looking up the appropriate actor class from data tables
		// 2. Spawning the actor at the anchor location
		// 3. Setting up replication properties
		// 4. Configuring actor-specific properties

		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, VeryVerbose, TEXT("[PCGDungeonSubSystem] Would spawn actor at anchor: %s, Location: %s"),
				*Anchor.Tag.ToString(), *Anchor.Location.ToString());
		}
	}
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

void UPCGDungeonSubSystem::NotifyPCGGenerationComplete()
{
	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PCGDungeonSubSystem] PCG generation completed - notifying listeners"));
	}

	// Broadcast completion event
	OnPCGGenerationComplete.Broadcast();

	// Generate gameplay anchors after PCG completion
	TArray<FPCGAnchorData> GameplayAnchors = GeneratePCGAnchors(CurrentSeedSet);
	
	if (AnchorSystem && GameplayAnchors.Num() > 0)
	{
		AnchorSystem->ProcessAnchors(GameplayAnchors);
	}
}

FVector UPCGDungeonSubSystem::FindSuitableRoomCenter() const
{
	// TODO: Implement actual room center finding logic
	// This would involve:
	// 1. Querying PCG generated rooms
	// 2. Finding the largest/safest room
	// 3. Returning its center point
	
	// For now, return a placeholder position
	FVector RoomCenter = FVector(
		FMath::RandRange(-500.0f, 500.0f),
		FMath::RandRange(-500.0f, 500.0f),
		FMath::RandRange(0.0f, 200.0f)
	);

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, VeryVerbose, TEXT("[PCGDungeonSubSystem] Found suitable room center: %s"), *RoomCenter.ToString());
	}

	return RoomCenter;
}

TArray<FVector> UPCGDungeonSubSystem::FindEnemySpawnPoints() const
{
	TArray<FVector> SpawnPoints;
	
	// TODO: Implement actual enemy spawn point finding logic
	// This would involve:
	// 1. Querying PCG generated corridors and small rooms
	// 2. Finding suitable hiding spots
	// 3. Ensuring proper spacing between spawn points
	
	// For now, generate placeholder spawn points
	int32 SpawnCount = FMath::RandRange(2, 5);
	for (int32 i = 0; i < SpawnCount; ++i)
	{
		FVector SpawnPoint = FVector(
			FMath::RandRange(-800.0f, 800.0f),
			FMath::RandRange(-800.0f, 800.0f),
			FMath::RandRange(0.0f, 300.0f)
		);
		SpawnPoints.Add(SpawnPoint);
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, VeryVerbose, TEXT("[PCGDungeonSubSystem] Found %d enemy spawn points"), SpawnPoints.Num());
	}

	return SpawnPoints;
}

TArray<FVector> UPCGDungeonSubSystem::FindHazardSpawnPoints() const
{
	TArray<FVector> SpawnPoints;
	
	// TODO: Implement actual hazard spawn point finding logic
	// This would involve:
	// 1. Querying PCG generated floor areas
	// 2. Finding strategic chokepoints
	// 3. Ensuring hazards don't block objectives
	
	// For now, generate placeholder spawn points
	int32 SpawnCount = FMath::RandRange(1, 3);
	for (int32 i = 0; i < SpawnCount; ++i)
	{
		FVector SpawnPoint = FVector(
			FMath::RandRange(-600.0f, 600.0f),
			FMath::RandRange(-600.0f, 600.0f),
			0.0f // Hazards are typically on the floor
		);
		SpawnPoints.Add(SpawnPoint);
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, VeryVerbose, TEXT("[PCGDungeonSubSystem] Found %d hazard spawn points"), SpawnPoints.Num());
	}

	return SpawnPoints;
}