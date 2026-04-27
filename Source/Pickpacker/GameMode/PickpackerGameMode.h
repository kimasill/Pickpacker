// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "PickpackerAssetPaths.h"
#include "PickpackerTypes/PickpackerTypes.h"
#include "PickpackerGameMode.generated.h"

class UAnchorRuntimeSubsystem;
class APlayerState;
class APickpackerGameState;
class UPCGDungeonSubSystem; // forward declaration
class UDA_OrderWaveData;
class UDA_TrainDestinationData;
class AParcelActor;

/**
 * Pickpacker Game Mode - Manages the warehouse simulation game
 * Players are robots working under "Mother" AI surveillance
 */
UCLASS()
class PICKPACKER_API APickpackerGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	APickpackerGameMode();

	virtual void BeginPlay() override;
	virtual void HandleMatchStart();

	/**
	 * Initialize the level with variant data
	 * @param LevelVariantData - Data asset containing level configuration
	 * @param Seed - Random seed for deterministic generation
	 */
	UFUNCTION(BlueprintCallable, Category = "Pickpacker|Level")
	void InitializeLevel(class UDA_LevelVariant* LevelVariantData, int32 Seed = 0);

	/**
	 * Start the warehouse simulation
	 */
	UFUNCTION(BlueprintCallable, Category = "Pickpacker|Gameplay")
	void StartWarehouseSimulation();

	/**
	 * End the warehouse simulation
	 */
	UFUNCTION(BlueprintCallable, Category = "Pickpacker|Gameplay")
	void EndWarehouseSimulation();

	/** 탈출 시퀀스 시작 시 오더 웨이브 중지 (탈출 과정 방해 방지) */
	void StopOrderWaves();

	/**
	 * Get current level variant data
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|Level")
	class UDA_LevelVariant* GetCurrentLevelVariant() const { return CurrentLevelVariant; }

	/**
	 * Get current seed
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|Level")
	int32 GetCurrentSeed() const { return CurrentSeed; }

	/**
	 * Check if simulation is running
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|Gameplay")
	bool IsSimulationRunning() const { return bSimulationRunning; }

	/**
	 * Handle player escape
	 */
	UFUNCTION(BlueprintCallable, Category = "Pickpacker|Gameplay")
	void OnPlayerEscaped(class ACharacter* Player);

	/**
	 * Handle all players escaped (victory)
	 */
	UFUNCTION(BlueprintCallable, Category = "Pickpacker|Gameplay")
	void OnAllPlayersEscaped();

	/**
	 * Handle game over (defeat)
	 */
	UFUNCTION(BlueprintCallable, Category = "Pickpacker|Gameplay")
	void OnGameOver(const FString& Reason);

	/** 게임 오버 진행 여부 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|GameOver")
	bool IsGameOverInProgress() const { return bGameOverInProgress; }

	/** SubmissionZone 도착 등으로 로비 복귀 트리거 */
	UFUNCTION(BlueprintCallable, Category = "Pickpacker|GameOver")
	void RequestReturnToLobby();

	virtual void PostSeamlessTravel() override;

	/**
	 * Called by clients' PlayerState to signal PCG readiness (Server Only)
	 */
	void RegisterClientPCGReady(APlayerState* PlayerState);

	/**
	 * Set mission configuration
	 * @param MissionId - The ID of the mission
	 * @param CustomSeed - Custom seed for the mission (optional)
	 */
	UFUNCTION(BlueprintCallable, Category = "Pickpacker|Gameplay")
	void SetMissionConfig(const FString& MissionId, int32 CustomSeed = 0);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|Gameplay")
	FMissionDefinition GetCurrentMissionDefinition() const { return CurrentMissionDefinition; }

	/**
	 * Called when a parcel enters the submission belt
	 */
	UFUNCTION(BlueprintCallable, Category = "Pickpacker|Orders")
	void ReportParcelSubmitted(AParcelActor* Parcel);

public:
	/** Wave data describing the sequence of parcel orders */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickpacker|Orders")
	UDA_OrderWaveData* OrderWaveData = nullptr;

	/** Interval (seconds) between order tick updates */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickpacker|Orders")
	float OrderUpdateInterval = 1.0f;

	/** Seconds to keep resolved orders visible before pruning */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickpacker|Orders")
	float OrderResolutionHoldTime = 8.0f;

	/** Maximum number of active orders (<=0 means unlimited) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickpacker|Orders")
	int32 MaxActiveOrders = 0;

	/** Initial delay before the first order wave starts (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickpacker|Orders", meta = (ClampMin = "0.0"))
	float InitialOrderStartDelay = 0.0f;

	/** Whether to start the order system automatically once gameplay begins */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickpacker|Orders")
	bool bAutoStartOrders = true;

	/** Starting team credits */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickpacker|Credits")
	int32 StartingTeamCredits = 30;

	/** Available train destinations registered at runtime */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickpacker|Train")
	UDA_TrainDestinationData* TrainDestinationData = nullptr;

	/** 게임 시작 시 로드할 스트리밍 레벨 이름 (예: controlroom). 패키징 빌드에서 텔레포트/참조 실패 방지 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickpacker|Level")
	TArray<FName> StreamingLevelNamesToLoadAtStart;

	/** Apply a credit delta (server only) */
	UFUNCTION(BlueprintCallable, Category = "Pickpacker|Credits")
	void ApplyCreditDelta(int32 Delta, const FString& Reason);

	/** Wave started event (Blueprint UI hook) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Pickpacker|Orders")
	void BP_OnOrderWaveStarted(int32 WaveNumber);

	/** 파슬 제출 시 Blueprint에서 사운드/이펙트 등 처리용 이벤트 (제출 직후, 파슬 파괴 전 브로드캐스트) */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnParcelSubmitted, AParcelActor*, Parcel, bool, bAccepted);
	UPROPERTY(BlueprintAssignable, Category = "Pickpacker|Orders")
	FOnParcelSubmitted OnParcelSubmitted;

	/** 게임 오버 연출 (선택) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Pickpacker|GameOver")
	void BP_PlayGameOverSequence(const FString& Reason);


protected:
	/**
	 * Called when match state is set
	 */
	virtual void OnMatchStateSet() override;

	/**
	 * Called when PCG generation is complete
	 */
	void OnPCGGenerationComplete(bool bSuccess);

	/**
	 * Start gameplay after initialization
	 */
	void StartGameplay();

	/**
	 * Check game end conditions
	 */
	void CheckGameEndConditions();

	/**
	 * Handle suspicion level changes
	 */
	UFUNCTION()
	void OnSuspicionChanged(float NewSuspicionLevel);

	/** Current level variant data */
	UPROPERTY()
	class UDA_LevelVariant* CurrentLevelVariant = nullptr;

	/** Current random seed */
	UPROPERTY()
	int32 CurrentSeed = 0;

	/** Whether simulation is currently running */
	UPROPERTY()
	bool bSimulationRunning = false;

	/** Anchor runtime subsystem reference */
	UPROPERTY()
	UAnchorRuntimeSubsystem* AnchorSubsystem = nullptr;

	// Mission settings
	UPROPERTY()
	FSeedSet CurrentMissionConfig;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickpacker|Gameplay", meta = (AllowPrivateAccess = "true"))
	FMissionDefinition CurrentMissionDefinition;

	// PCG subsystem (not reflected)
	UPCGDungeonSubSystem* PCGDungeonSubsystem = nullptr;

	// Cached game state
	UPROPERTY()
	APickpackerGameState* PickpackerGameState = nullptr;

	// Generation flag
	bool bPCGGenerationInProgress = false;

	/** Game end check timer */
	FTimerHandle GameEndCheckTimer;

	/** 게임 오버 후 로비 이동 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickpacker|GameOver")
	bool bReturnToLobbyOnGameOver = true;

	/** 게임 오버 시 로비 이동을 SubmissionZone 도착까지 지연 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickpacker|GameOver")
	bool bDeferLobbyReturnUntilSubmissionZone = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickpacker|GameOver")
	float GameOverReturnDelay = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickpacker|GameOver")
	FString LobbyTravelPath = PickpackerAssetPaths::Maps::EntryMap;

	FTimerHandle ReturnToLobbyTimerHandle;
	bool bGameOverInProgress = false;
	bool bReturnToLobbyTriggered = false;

	void ReturnPlayersToLobby();

	/** Game over event */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameOver, const FString&, Reason);
	UPROPERTY(BlueprintAssignable, Category = "Pickpacker|Events")
	FOnGameOver OnGameOverEvent;
protected:
	/** Order system helpers */
	void StartOrderSystem();
	void BeginOrderWave(int32 WaveIndex, int32 RepeatIndex = 0);
	void ScheduleNextOrderWave(int32 WaveIndex, int32 RepeatIndex, float DelaySeconds);
	void TickOrderSystem();
	void CleanupResolvedOrders();
	bool AreAllOrdersResolved() const;
	bool TryFulfillOrders(AParcelActor* Parcel);
	void HandleOrderFailure(FActiveOrderState& Order, const FString& Reason);
	void HandleOrderSuccess(FActiveOrderState& Order);
	void SyncOrdersToGameState();
	void ApplyOrderPenalty(const FActiveOrderState& Order) const;
	void HandleNextOrderWaveTimer();
	bool CanSpawnNewOrders() const;
	void RegisterTrainDestinations();
	void RefreshMissionDefinitionFromOrders();
	void TryAdvanceCoreLoopFromOrders();
	bool TryRestoreTravelSnapshot();
	void UnloadTrainCargoToStorage(const TArray<FStorageRecord>& CargoRecords);

protected:
	/** Current orders tracked on the server */
	UPROPERTY()
	TArray<FActiveOrderState> ActiveOrders;

	/** Whether the order system has been initialized */
	bool bOrderSystemInitialized = false;

	/** Current wave index */
	int32 CurrentOrderWaveIndex = INDEX_NONE;

	/** Current repeat index within the wave */
	int32 CurrentWaveRepeatIndex = 0;

	/** Pending wave info for timer */
	int32 PendingWaveIndex = INDEX_NONE;
	int32 PendingWaveRepeatIndex = 0;

	/** Order system timers */
	FTimerHandle OrderSystemTimerHandle;
	FTimerHandle NextWaveTimerHandle;

	/** Prevent duplicate train boarding triggers once the work loop is complete */
	bool bTrainBoardingTriggered = false;
};
