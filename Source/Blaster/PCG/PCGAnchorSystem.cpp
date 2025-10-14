// Fill out your copyright notice in the Description page of Project Settings.

#include "PCGAnchorSystem.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

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

void UPCGAnchorSystem::SetWorldContext(UWorld* InWorld)
{
	World = InWorld;
}

bool UPCGAnchorSystem::ProcessAnchors(const TArray<FPCGAnchorData>& Anchors)
{
	// World is only required if we actually spawn. For tag-consumption mode, we can proceed without World.
	if (bAllowRuntimeSpawnBySystem && !World)
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
			if (bAllowRuntimeSpawnBySystem && ObjectivesDataTable)
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
			else
			{
				// Tag-consumption only: just count
				ObjectiveCount++;
			}
		}
		// Handle extract points
		else if (Anchor.AnchorType == EPCGAnchorType::Extract)
		{
			if (bAllowRuntimeSpawnBySystem && ObjectivesDataTable)
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
			else
			{
				ExtractCount++;
			}
		}
		// Handle spawners
		else if (bAllowRuntimeSpawnBySystem && SpawnersDataTable)
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
		else
		{
			// Tag-consumption: count only
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
			default:
				break;
			}
		}
	}

	// Validate placement
	bool bValidationPassed = ValidateAnchorPlacement(Anchors);
	
	UE_LOG(LogTemp, Log, TEXT("[PCGAnchorSystem] Processing complete - Objectives: %d, Extracts: %d, Enemies: %d, Hazards: %d, Items: %d"),
		ObjectiveCount, ExtractCount, EnemySpawnCount, HazardSpawnCount, ItemSpawnCount);

	return bValidationPassed;
}

TArray<FPCGAnchorData> UPCGAnchorSystem::ScanWorldForPCGTags() const
{
	TArray<FPCGAnchorData> Anchors;
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PCGAnchorSystem] World not set - cannot scan for PCG tags"));
		return Anchors;
	}

	// Collect all actors and inspect tags
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor) continue;

		EPCGAnchorType DetectedType = EPCGAnchorType::None;
		FName DetectedTag = NAME_None;

		for (const FName& TagName : Actor->Tags)
		{
			const FString TagStr = TagName.ToString();

			if (TagStr.Equals(TEXT("PCG.Anchor.Objective"), ESearchCase::IgnoreCase))
			{
				DetectedType = EPCGAnchorType::Objective;
				DetectedTag = FName(TEXT("Objective"));
				break;
			}
			if (TagStr.Equals(TEXT("PCG.Anchor.Extract"), ESearchCase::IgnoreCase))
			{
				DetectedType = EPCGAnchorType::Extract;
				DetectedTag = FName(TEXT("Extract"));
				break;
			}
			if (TagStr.Equals(TEXT("PCG.Spawner.Enemy"), ESearchCase::IgnoreCase))
			{
				DetectedType = EPCGAnchorType::EnemySpawn;
				DetectedTag = FName(TEXT("Enemy"));
				break;
			}
			if (TagStr.Equals(TEXT("PCG.Spawner.Hazard"), ESearchCase::IgnoreCase))
			{
				DetectedType = EPCGAnchorType::HazardSpawn;
				DetectedTag = FName(TEXT("Hazard"));
				break;
			}
			if (TagStr.Equals(TEXT("PCG.Spawner.Item"), ESearchCase::IgnoreCase))
			{
				DetectedType = EPCGAnchorType::ItemSpawn;
				DetectedTag = FName(TEXT("Item"));
				break;
			}
		}

		if (DetectedType != EPCGAnchorType::None)
		{
			FPCGAnchorData Data;
			Data.AnchorType = DetectedType;
			Data.Tag = DetectedTag;
			Data.Location = Actor->GetActorLocation();
			Data.Rotation = Actor->GetActorRotation();

			// Optionally capture actor name/id
			Data.Metadata.Add(TEXT("ActorName"), Actor->GetName());
			Anchors.Add(MoveTemp(Data));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[PCGAnchorSystem] Scanned world and found %d PCG-tagged anchors"), Anchors.Num());
	return Anchors;
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


