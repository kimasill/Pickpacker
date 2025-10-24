// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Blaster/Subsystem/PCGDungeonSubSystem.h"
#include "PickpackerGameMode.generated.h"

class APlayerController;
class AActor;
class APlayerStart;
class APlayerState;

UCLASS()
class BLASTER_API APickpackerGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	APickpackerGameMode();

	virtual void BeginPlay() override;
	virtual void OnMatchStateSet() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	UFUNCTION(BlueprintCallable, Category = "Pickpacker")
	void HandleMatchStart();

	UFUNCTION(BlueprintCallable, Category = "Pickpacker")
	void SetMissionConfig(const FString& MissionId, int32 CustomSeed = 0);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker")
	const FSeedSet& GetMissionConfig() const { return CurrentMissionConfig; }

	// Expose gameplay start so subsystem can safely trigger it after finalize
	UFUNCTION()
	void StartGameplay();

	// Server-only: called when GS triggers client PCG run; captures expected client count snapshot
	void OnServerTriggerClientPCGRun(int32 ExpectedClients);

	// Server-only: called when a client reports PCG ready
	void RegisterClientPCGReady(APlayerState* ReportingPS);

protected:
	UFUNCTION()
	void OnPCGGenerationComplete();

	// Cache PCG-provided spawn points and transforms
	void CachePCGPlayerStarts();
	void CachePCGPlayerSpawnTransformsFromAnchors();

	// Fallback: scan PlayerStart only when no PCG spawns available
	void SelectRandomPlayerStarts();

private:
	UPROPERTY()
	FSeedSet CurrentMissionConfig;

	UPROPERTY()
	bool bPCGGenerationInProgress = false;

	UPROPERTY()
	bool bPCGReady = false;

	// Prevent double-start / double-spawn
	UPROPERTY(Transient)
	bool bGameplayStarted = false;

	UPROPERTY()
	UPCGDungeonSubSystem* PCGDungeonSubsystem = nullptr;

	UPROPERTY()
	TArray<TWeakObjectPtr<APlayerController>> PendingSpawnControllers;

	// AActor-based spawn points (from PCG tags)
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> SelectedSpawnPoints;

	// Transform-based spawn points from GeneratePCGAnchors
	UPROPERTY()
	TArray<FTransform> SelectedSpawnTransforms;

	int32 NextSpawnIndex = 0;
	int32 NextTransformIndex = 0;

	// Client readiness tracking (server-only)
	UPROPERTY(Transient)
	int32 ClientsPCGReadyCount_Server = 0;

	UPROPERTY(Transient)
	int32 ExpectedClientCountSnapshot_Server = -1;

	UPROPERTY(Transient)
	bool bAllClientsPCGReadyTriggered_Server = false;

	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<APlayerState>> ReadyPlayers_Server;
};
















