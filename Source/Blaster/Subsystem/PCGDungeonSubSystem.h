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
class AActor;

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
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnClientPCGComplete);

	/**
	 * PCG generation complete event
	 */
	UPROPERTY(BlueprintAssignable, Category = "PCG Dungeon")
	FOnPCGGenerationComplete OnPCGGenerationComplete;

	UPROPERTY(BlueprintAssignable, Category = "PCG Dungeon")
	FOnClientPCGComplete OnClientPCGComplete;

	/**
	 * Notify PCG generation completion
	 */
	UFUNCTION(BlueprintCallable, Category = "PCG Dungeon")
	void NotifyPCGGenerationComplete(UPCGComponent* InPCG);

	// Called on clients to indicate they have finished local PCG
	UFUNCTION(BlueprintCallable, Category = "PCG Dungeon|Client")
	void MarkClientPCGReady();

private:
	// Retry timer handle for waiting until proper PlayerController exists on client
	FTimerHandle ClientReadyRetryHandle;
	void TryReportClientReady();

	// Short grace delay after server PCG completes to allow late-joining clients before finalizing
	FTimerHandle FinalizeWaitHandle;
	void MaybeFinalizeAfterWait();

	// Failsafe: force finalize and start gameplay if clients didn't report within timeout
	FTimerHandle ForceFinalizeHandle;
	void ForceFinalizeAfterTimeout();

	/**
	 * Get PCG Anchor System
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "PCG Dungeon")
	UPCGAnchorSystem* GetAnchorSystem() const { return AnchorSystem; }

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

	// Let Blueprint drive PCG execution instead of C++ (BP_Dungeon, etc.)
	UPROPERTY(EditAnywhere, Category = "PCG Dungeon")
	bool bUseBlueprintPCGControl = true;

	// Auto-bounds settings for placed PCG components
	UPROPERTY(EditAnywhere, Category = "PCG Dungeon|Bounds")
	bool bAutoCreateBoundsIfMissing = true;

	UPROPERTY(EditAnywhere, Category = "PCG Dungeon|Bounds")
	FVector AutoBoundsExtent = FVector(5000.f, 5000.f, 2000.f);

	// Client preview-only PCG: run PCG for visuals only (no actor spawns in BP graph). Server always authoritative for gameplay.
	UPROPERTY(EditAnywhere, Category = "PCG Dungeon|Client")
	bool bClientPreviewOnly = true;

	// Client minimize scan cost: collect only PlayerSpawnPoint on client in NotifyPCGGenerationComplete
	UPROPERTY(EditAnywhere, Category = "PCG Dungeon|Client")
	bool bClientScanPlayerOnly = true;

	// Expose cached spawn point accessors
	UFUNCTION(BlueprintCallable, Category = "PCG Dungeon|Spawns")
	void GetSpawnPointsByTag(FName SpawnTag, TArray<AActor*>& OutActors) const;

	UFUNCTION(BlueprintCallable, Category = "PCG Dungeon|Spawns")
	void GetPlayerSpawnPoints(TArray<AActor*>& OutActors) const { GetSpawnPointsByTag(FName("PlayerSpawnPoint"), OutActors); }

public:
	// Called after all clients finished their local PCG run to finalize on server
	UFUNCTION(BlueprintCallable, Category = "PCG Dungeon|Server")
	void ServerFinalizePCG();

protected:
	// Current seed set
	UPROPERTY()
	FSeedSet CurrentSeedSet;

	// Generation state
	UPROPERTY()
	bool bIsGenerating = false;

	UPROPERTY()
	bool bGenerationComplete = false;

	// True if this local world ran PCG as a client-side local sim (for dev flow)
	UPROPERTY()
	bool bClientLocalPCG = false;

	// Server: waiting for clients to finish before finalizing
	UPROPERTY()
	bool bAwaitingClientsForFinalize = false;

	// PCG Plugin Integration
	UPROPERTY()
	class UPCGComponent* PCGComponent;

	UPROPERTY()
	class UPCGGraph* PCGGraph;

	// Generation timer
	UPROPERTY()
	float GenerationStartTime = 0.0f;

	// Cache from PCG
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> CachedGeneratedActors;

	UPROPERTY()
	TWeakObjectPtr<UPCGComponent> LastPCGComponent;

	// Blueprint should collect generated actors from the PCG component and return them
	UFUNCTION(BlueprintImplementableEvent, Category = "PCG Dungeon|PCG")
	void CollectPCGGeneratedActors(UPCGComponent* InPCG, TArray<AActor*>& OutActors);

	UFUNCTION(BlueprintCallable, Category = "PCG Dungeon|PCG")
	void ClearCachedPCGActors() { CachedGeneratedActors.Reset(); }

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
};