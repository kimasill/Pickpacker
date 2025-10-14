// Fill out your copyright notice in the Description page of Project Settings.

#include "PCGAnchorSystem.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "Kismet/GameplayStatics.h"

UPCGAnchorSystem::UPCGAnchorSystem()
{
	World = nullptr;
	ObjectiveCount = 0;
	ExtractCount = 0;
	EnemySpawnCount = 0;
	HazardSpawnCount = 0;
	ItemSpawnCount = 0;
	bHasMinimumObjectives = false;
	bHasMinimumExtracts = false;
}

void UPCGAnchorSystem::Initialize(UDataTable* ObjectivesTable, UDataTable* SpawnersTable)
{
	ObjectivesDataTable = ObjectivesTable;
	SpawnersDataTable = SpawnersTable;

	UE_LOG(LogTemp, Log, TEXT("[PCGAnchorSystem] Initialized with objectives table: %s, spawners table: %s"),
		ObjectivesTable ? *ObjectivesTable->GetName() : TEXT("None"),
		SpawnersTable ? *SpawnersTable->GetName() : TEXT("None"));
}

void UPCGAnchorSystem::SetDataTables(UDataTable* ObjectivesTable, UDataTable* SpawnersTable)
{
	ObjectivesDataTable = ObjectivesTable;
	SpawnersDataTable = SpawnersTable;

	UE_LOG(LogTemp, Log, TEXT("[PCGAnchorSystem] Data tables updated"));
}

bool UPCGAnchorSystem::ProcessAnchors(const TArray<FPCGAnchorData>& Anchors)
{
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[PCGAnchorSystem] World not set - cannot spawn actors"));
		return false;
	}

	// Reset counters
	ObjectiveCount = 0;
	ExtractCount = 0;
	EnemySpawnCount = 0;
	HazardSpawnCount = 0;
	ItemSpawnCount = 0;

	UE_LOG(LogTemp, Log, TEXT("[PCGAnchorSystem] Processing %d anchors"), Anchors.Num());

	// Process each anchor
	for (const FPCGAnchorData& Anchor : Anchors)
	{
		if (Anchor.AnchorType == EPCGAnchorType::None)
		{
			continue;
		}

		// Handle objectives
		if (Anchor.AnchorType == EPCGAnchorType::Objective)
		{
			if (ObjectivesDataTable)
			{
				FObjectiveRow* ObjectiveRow = ObjectivesDataTable->FindRow<FObjectiveRow>(Anchor.Tag, TEXT(""));
				if (ObjectiveRow && ObjectiveRow->Class)
				{
					AActor* SpawnedActor = SpawnObjectiveActor(Anchor, *ObjectiveRow);
					if (SpawnedActor)
					{
						ObjectiveCount++;
						UE_LOG(LogTemp, Log, TEXT("[PCGAnchorSystem] Spawned objective: %s at %s"),
							*Anchor.Tag.ToString(), *Anchor.Location.ToString());
					}
				}
			}
		}
		// Handle extract points
		else if (Anchor.AnchorType == EPCGAnchorType::Extract)
		{
			if (ObjectivesDataTable)
			{
				FObjectiveRow* ExtractRow = ObjectivesDataTable->FindRow<FObjectiveRow>(Anchor.Tag, TEXT(""));
				if (ExtractRow && ExtractRow->Class)
				{
					AActor* SpawnedActor = SpawnObjectiveActor(Anchor, *ExtractRow);
					if (SpawnedActor)
					{
						ExtractCount++;
						UE_LOG(LogTemp, Log, TEXT("[PCGAnchorSystem] Spawned extract point: %s at %s"),
							*Anchor.Tag.ToString(), *Anchor.Location.ToString());
					}
				}
			}
		}
		// Handle spawners
		else if (SpawnersDataTable)
		{
			FSpawnerRow* SpawnerRow = SpawnersDataTable->FindRow<FSpawnerRow>(Anchor.Tag, TEXT(""));
			if (SpawnerRow && SpawnerRow->Class)
			{
				// Check spawn chance
				if (FMath::RandRange(0.0f, 1.0f) <= SpawnerRow->SpawnChance)
				{
					AActor* SpawnedActor = SpawnSpawnerActor(Anchor, *SpawnerRow);
					if (SpawnedActor)
					{
						switch (Anchor.AnchorType)
						{
						case EPCGAnchorType::EnemySpawn:
							EnemySpawnCount++;
							break;
						case EPCGAnchorType::HazardSpawn:
							HazardSpawnCount++;
							break;
						case EPCGAnchorType::ItemSpawn:
							ItemSpawnCount++;
							break;
						}
						
						UE_LOG(LogTemp, Log, TEXT("[PCGAnchorSystem] Spawned %s: %s at %s"),
							*UEnum::GetValueAsString(Anchor.AnchorType),
							*Anchor.Tag.ToString(), *Anchor.Location.ToString());
					}
				}
			}
		}
	}

	// Validate placement
	bool bValidationPassed = ValidateAnchorPlacement(Anchors);
	
	UE_LOG(LogTemp, Log, TEXT("[PCGAnchorSystem] Processing complete - Objectives: %d, Extracts: %d, Enemies: %d, Hazards: %d, Items: %d"),
		ObjectiveCount, ExtractCount, EnemySpawnCount, HazardSpawnCount, ItemSpawnCount);

	return bValidationPassed;
}

bool UPCGAnchorSystem::ValidateAnchorPlacement(const TArray<FPCGAnchorData>& Anchors)
{
	bHasMinimumObjectives = ObjectiveCount >= 1;
	bHasMinimumExtracts = ExtractCount >= 1;

	LogValidationResults(Anchors);

	return bHasMinimumObjectives && bHasMinimumExtracts;
}

int32 UPCGAnchorSystem::GetObjectiveCount(EPCGAnchorType AnchorType) const
{
	switch (AnchorType)
	{
	case EPCGAnchorType::Objective:
		return ObjectiveCount;
	case EPCGAnchorType::Extract:
		return ExtractCount;
	case EPCGAnchorType::EnemySpawn:
		return EnemySpawnCount;
	case EPCGAnchorType::HazardSpawn:
		return HazardSpawnCount;
	case EPCGAnchorType::ItemSpawn:
		return ItemSpawnCount;
	default:
		return 0;
	}
}

int32 UPCGAnchorSystem::GetSpawnerCount(EPCGAnchorType AnchorType) const
{
	return GetObjectiveCount(AnchorType);
}

AActor* UPCGAnchorSystem::SpawnObjectiveActor(const FPCGAnchorData& Anchor, const FObjectiveRow& Row)
{
	if (!World || !Row.Class)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	SpawnParams.bNoFail = true;

	AActor* SpawnedActor = World->SpawnActor<AActor>(Row.Class, Anchor.Location, Anchor.Rotation, SpawnParams);
	
	if (SpawnedActor)
	{
		// Configure replication
		SpawnedActor->SetReplicates(true);
		SpawnedActor->SetReplicateMovement(true);
		
		// Add PCG anchor tag
		SpawnedActor->Tags.Add(FName(TEXT("PCG.Anchor")));
		SpawnedActor->Tags.Add(Anchor.Tag);
		
		// Add anchor type tag
		FString AnchorTypeString = UEnum::GetValueAsString(Anchor.AnchorType);
		SpawnedActor->Tags.Add(FName(*AnchorTypeString));
	}

	return SpawnedActor;
}

AActor* UPCGAnchorSystem::SpawnSpawnerActor(const FPCGAnchorData& Anchor, const FSpawnerRow& Row)
{
	if (!World || !Row.Class)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	SpawnParams.bNoFail = true;

	AActor* SpawnedActor = World->SpawnActor<AActor>(Row.Class, Anchor.Location, Anchor.Rotation, SpawnParams);
	
	if (SpawnedActor)
	{
		// Configure replication
		SpawnedActor->SetReplicates(true);
		SpawnedActor->SetReplicateMovement(true);
		
		// Add PCG spawner tag
		SpawnedActor->Tags.Add(FName(TEXT("PCG.Spawner")));
		SpawnedActor->Tags.Add(Anchor.Tag);
		
		// Add spawner type tag
		FString AnchorTypeString = UEnum::GetValueAsString(Anchor.AnchorType);
		SpawnedActor->Tags.Add(FName(*AnchorTypeString));
	}

	return SpawnedActor;
}

void UPCGAnchorSystem::LogValidationResults(const TArray<FPCGAnchorData>& Anchors)
{
	UE_LOG(LogTemp, Log, TEXT("[PCGAnchorSystem] Validation Results:"));
	UE_LOG(LogTemp, Log, TEXT("  - Objectives: %d (Required: >=1, Pass: %s)"), 
		ObjectiveCount, bHasMinimumObjectives ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogTemp, Log, TEXT("  - Extract Points: %d (Required: >=1, Pass: %s)"), 
		ExtractCount, bHasMinimumExtracts ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogTemp, Log, TEXT("  - Enemy Spawners: %d"), EnemySpawnCount);
	UE_LOG(LogTemp, Log, TEXT("  - Hazard Spawners: %d"), HazardSpawnCount);
	UE_LOG(LogTemp, Log, TEXT("  - Item Spawners: %d"), ItemSpawnCount);
	UE_LOG(LogTemp, Log, TEXT("  - Total Anchors Processed: %d"), Anchors.Num());
	
	if (!bHasMinimumObjectives || !bHasMinimumExtracts)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PCGAnchorSystem] Validation FAILED - Missing required anchor types"));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[PCGAnchorSystem] Validation PASSED - All requirements met"));
	}
}


