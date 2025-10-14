// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Blaster/PickpackerTypes/PickpackerTypes.h"
#include "Blaster/PCG/PCGAnchorSystem.h"
#include "Engine/DataTable.h"

// Forward declare PCG plugin classes to avoid heavy includes in header
class UPCGComponent;
class UPCGGraph;

#include "PCGDungeonSubSystem.generated.h"

/**
 * PCG Dungeon Subsystem - Manages server-only PCG generation
 */
UCLASS()
class BLASTER_API UPCGDungeonSubSystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UPCGDungeonSubSystem();

	/**
	 * Generate dungeon using PCG Plugin
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "PCG Dungeon")
	void GenerateDungeon(const FSeedSet& SeedSet);

	/**
	 * Check if PCG generation is allowed (server only)
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "PCG Dungeon")
	bool IsPCGGenerationAllowed() const;

	/**
	 * Get current seed set
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "PCG Dungeon")
	FSeedSet GetCurrentSeedSet() const;

	/**
	 * Set data tables for PCG generation
	 */
	UFUNCTION(BlueprintCallable, Category = "PCG Dungeon")
	void SetDataTables(UDataTable* ObjectivesTable, UDataTable* SpawnersTable);

	/**
	 * Generate PCG anchors using PCG Plugin
	 */
	UFUNCTION(BlueprintCallable, Category = "PCG Dungeon")
	TArray<FPCGAnchorData> GeneratePCGAnchors(const FSeedSet& SeedSet);

	/**
	 * Load PCG Level from Plugin
	 */
	UFUNCTION(BlueprintCallable, Category = "PCG Dungeon")
	bool LoadPCGLevel(const FString& LevelName = TEXT("PCG_MultiFloorDungeon"));

	/**
	 * Execute a PCG graph by asset name
	 */
	bool ExecutePCGGraph(const FString& GraphName);

	/**
	 * PCG generation complete delegate
	 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPCGGenerationComplete);

	/**
	 * PCG generation complete event
	 */
	UPROPERTY(BlueprintAssignable, Category = "PCG Dungeon")
	FOnPCGGenerationComplete OnPCGGenerationComplete;

	/**
	 * Notify PCG generation completion
	 */
	UFUNCTION(BlueprintCallable, Category = "PCG Dungeon")
	void NotifyPCGGenerationComplete();

	/**
	 * Find suitable room center for objectives
	 */
	UFUNCTION(BlueprintCallable, Category = "PCG Dungeon")
	FVector FindSuitableRoomCenter() const;

	/**
	 * Find enemy spawn points in generated dungeon
	 */
	UFUNCTION(BlueprintCallable, Category = "PCG Dungeon")
	TArray<FVector> FindEnemySpawnPoints() const;

	/**
	 * Find hazard spawn points in generated dungeon
	 */
	UFUNCTION(BlueprintCallable, Category = "PCG Dungeon")
	TArray<FVector> FindHazardSpawnPoints() const;

	/**
	 * Get PCG Anchor System
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "PCG Dungeon")
	UPCGAnchorSystem* GetAnchorSystem() const { return AnchorSystem; }

protected:
	/**
	 * Internal dungeon generation logic
	 */
	void InternalGenerateDungeon(const FSeedSet& SeedSet);

	/**
	 * Spawn PCG actors with proper replication
	 */
	void SpawnPCGActors(const TArray<FPCGAnchorData>& Anchors);

	/**
	 * Validate PCG generation results
	 */
	bool ValidatePCGGeneration(const TArray<FPCGAnchorData>& Anchors) const;

	/**
	 * Log PCG generation results
	 */
	void LogPCGGenerationResults(const TArray<FPCGAnchorData>& Anchors) const;

public:
	// PCG Anchor System
	UPROPERTY(BlueprintReadOnly, Category = "PCG Dungeon")
	UPCGAnchorSystem* AnchorSystem;

	// Data Tables
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PCG Dungeon")
	UDataTable* ObjectivesDataTable;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PCG Dungeon")
	UDataTable* SpawnersDataTable;

	// PCG Level Settings
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PCG Dungeon")
	FString PCGLevelName = TEXT("PCG_MultiFloorDungeon");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PCG Dungeon")
	FString PCGGraphName = TEXT("PCG_MultiFloorDungeon");

	// Generation Settings
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PCG Dungeon")
	bool bEnableDebugLogging = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PCG Dungeon")
	bool bValidateGeneration = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PCG Dungeon")
	float GenerationTimeout = 30.0f;

protected:
	// Current seed set
	UPROPERTY()
	FSeedSet CurrentSeedSet;

	// Generation state
	UPROPERTY()
	bool bIsGenerating = false;

	UPROPERTY()
	bool bGenerationComplete = false;

	// PCG Plugin Integration
	UPROPERTY()
	class UPCGComponent* PCGComponent;

	UPROPERTY()
	class UPCGGraph* PCGGraph;

	// Generation timer
	UPROPERTY()
	float GenerationStartTime = 0.0f;
};