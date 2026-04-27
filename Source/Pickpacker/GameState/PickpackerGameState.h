// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "PickpackerTypes/PickpackerTypes.h"
#include "PickpackerTypes/CoreLoopTypes.h"
#include "DataAssets/DA_LevelVariant.h"
#include "Components/EscapeProgressComponent.h"
#include "Components/TrainTravelComponent.h"
#include "PickpackerGameState.generated.h"

class UAnchorRuntimeSubsystem;

/** Payload for order remaining time updates (OrderId -> RemainingSeconds) */
USTRUCT(BlueprintType)
struct FOrderTimesUpdatePayload
{
	GENERATED_BODY()

	/** Map of OrderId to RemainingSeconds (-1 if no time limit) */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FGuid OrderId;
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float RemainingSeconds = -1.0f;
};

/**
 * Pickpacker Game State - Manages warehouse simulation state
 * Tracks suspicion levels, team performance, and game progression
 */
UCLASS()
class PICKPACKER_API APickpackerGameState : public AGameState
{
	GENERATED_BODY()

public:
	APickpackerGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|Ending")
	UEscapeProgressComponent* GetEscapeProgressComponent() const { return EscapeProgressComponent; }

	/** 열차 이동 관리 컴포넌트 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|Train")
	UTrainTravelComponent* GetTrainTravelComponent() const { return TrainTravelComponent; }

	/**
	 * Set the level variant data (Server Only)
	 */
	UFUNCTION(BlueprintCallable, Category = "Pickpacker")
	void SetLevelVariant(UDA_LevelVariant* NewLevelVariant);

	/**
	 * Get current level variant
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker")
	UDA_LevelVariant* GetLevelVariant() const { return LevelVariant; }

	/**
	 * Set the random seed (Server Only)
	 */
	UFUNCTION(BlueprintCallable, Category = "Pickpacker")
	void SetRandomSeed(int32 NewSeed);

	/**
	 * Get current random seed
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker")
	int32 GetRandomSeed() const { return RandomSeed; }

	/**
	 * Add suspicion points to the team
	 */
	UFUNCTION(BlueprintCallable, Category = "Pickpacker|Suspicion")
	void AddTeamSuspicion(float SuspicionPoints);

	UFUNCTION(BlueprintCallable, Category = "Pickpacker|Suspicion")
	void SetTeamSuspicionValue(float NewSuspicion);

	/**
	 * Get current team suspicion level
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|Suspicion")
	float GetTeamSuspicion() const { return TeamSuspicion; }

	/**
	 * Get suspicion level (0.0 - 1.0)
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|Suspicion")
	float GetSuspicionLevel() const;

	/** 드랍 의심 판정 내구도 소모 비율 임계치 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|Suspicion")
	float GetParcelDropSuspicionDamageRatioThreshold() const { return ParcelDropSuspicionDamageRatioThreshold; }

	/**
	 * Check if simulation is running
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker")
	bool IsSimulationRunning() const { return bSimulationRunning; }

	/**
	 * Set simulation running state
	 */
	UFUNCTION(BlueprintCallable, Category = "Pickpacker")
	void SetSimulationRunning(bool bRunning);

	/**
	 * Get anchor runtime subsystem
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker")
	UAnchorRuntimeSubsystem* GetAnchorSubsystem();

	/**
	 * Get current replicated orders
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|Orders")
	const TArray<FActiveOrderState>& GetActiveOrders() const { return ActiveOrders; }

	/** Server-side setter for active orders. Starts automatic remaining-time updates. */
	void SetActiveOrders(const TArray<FActiveOrderState>& NewOrders);

	/** Set current order wave number (server only) */
	UFUNCTION(BlueprintCallable, Category = "Pickpacker|Orders")
	void SetCurrentOrderWaveNumber(int32 NewWaveNumber);

	/** Get current order wave number */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|Orders")
	int32 GetCurrentOrderWaveNumber() const { return CurrentOrderWaveNumber; }

	/**
	 * Set team credits (server only)
	 */
	UFUNCTION(BlueprintCallable, Category = "Pickpacker|Credits")
	void SetTeamCredits(int32 NewCredits);

	/**
	 * Apply delta to team credits (server only)
	 */
	UFUNCTION(BlueprintCallable, Category = "Pickpacker|Credits")
	void ApplyCreditDelta(int32 Delta, const FString& Reason);

	/**
	 * Get current team credits
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|Credits")
	int32 GetTeamCredits() const { return TeamCredits; }

	/**
	 * Get credit transaction history
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|Credits")
	const TArray<FCreditTransaction>& GetCreditHistory() const { return CreditHistory; }

	/**
	 * 게임 시간 시스템
	 */
	
	/** 현재 게임 시간(시간) 가져오기 (0.0 ~ 24.0) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|Time")
	float GetCurrentGameHour() const;

	/** 게임 시간(시간)을 실제 시간(초)로 변환 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|Time")
	float ConvertGameHoursToRealSeconds(float GameHours) const;

	/** 실제 시간(초)을 게임 시간(시간)으로 변환 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|Time")
	float ConvertRealSecondsToGameHours(float RealSeconds) const;

	/** 게임 시간 속도 설정 (예: 1.0 = 정상 속도, 2.0 = 2배 속도) */
	UFUNCTION(BlueprintCallable, Category = "Pickpacker|Time")
	void SetGameTimeSpeed(float Speed);

	/** 게임 시간 속도 가져오기 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|Time")
	float GetGameTimeSpeed() const { return GameTimeSpeed; }

	/** 하루 길이 설정 (실제 시간 초 단위, 기본값: 1440초 = 24분) */
	UFUNCTION(BlueprintCallable, Category = "Pickpacker|Time")
	void SetDayLengthInSeconds(float Seconds);

	/** 하루 길이 가져오기 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|Time")
	float GetDayLengthInSeconds() const { return DayLengthInSeconds; }

	// --- Core Loop Phase (replicated) ---

	/** Set core loop phase (server only, called by CoreLoopSubsystem) */
	UFUNCTION(BlueprintCallable, Category = "Pickpacker|CoreLoop")
	void SetCoreLoopPhase(ECoreLoopPhase NewPhase);

	/** Get current core loop phase */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|CoreLoop")
	ECoreLoopPhase GetCoreLoopPhase() const { return CoreLoopPhase; }

	/** Set completed trips count (server only) */
	UFUNCTION(BlueprintCallable, Category = "Pickpacker|CoreLoop")
	void SetCompletedTrips(int32 NewTrips);

	/** Get completed trip count */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|CoreLoop")
	int32 GetCompletedTrips() const { return CompletedTrips; }

	UFUNCTION(BlueprintCallable, Category = "Pickpacker|CoreLoop")
	void SetRunStage(ERunProgressStage NewStage);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|CoreLoop")
	ERunProgressStage GetRunStage() const { return RunStage; }

	UFUNCTION(BlueprintCallable, Category = "Pickpacker|Mission")
	void SetCurrentMissionDefinition(const FMissionDefinition& NewMissionDefinition);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|Mission")
	const FMissionDefinition& GetCurrentMissionDefinition() const { return CurrentMissionDefinition; }

	UFUNCTION(BlueprintCallable, Category = "Pickpacker|Travel")
	void SetCurrentRouteSelectionResult(const FRouteSelectionResult& NewRouteSelectionResult);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|Travel")
	const FRouteSelectionResult& GetCurrentRouteSelectionResult() const { return CurrentRouteSelectionResult; }

	UFUNCTION(BlueprintCallable, Category = "Pickpacker|Storage")
	void SetSessionStorageRecords(const TArray<FStorageRecord>& NewStorageRecords);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|Storage")
	const TArray<FStorageRecord>& GetSessionStorageRecords() const { return SessionStorageRecords; }

	UFUNCTION(BlueprintCallable, Category = "Pickpacker|Meta")
	void SetRunPersonaStats(const FPersonaStats& NewPersonaStats);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|Meta")
	const FPersonaStats& GetRunPersonaStats() const { return RunPersonaStats; }

	UFUNCTION(BlueprintCallable, Category = "Pickpacker|Meta")
	void SetEndingFlagStates(const TArray<FEndingFlagState>& NewEndingFlagStates);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|Meta")
	const TArray<FEndingFlagState>& GetEndingFlagStates() const { return EndingFlagStates; }

public:
	// Alias to allow templated type with comma in delegate macro
	typedef TMap<FGuid, float> FOrderTimesMap;

	/** Broadcast when suspicion level changes */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSuspicionChanged, float, NewSuspicionLevel);
	UPROPERTY(BlueprintAssignable, Category = "Pickpacker|Suspicion")
	FOnSuspicionChanged OnSuspicionChanged;

	/** Broadcast when simulation state changes */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSimulationStateChanged, bool, bIsRunning);
	UPROPERTY(BlueprintAssignable, Category = "Pickpacker|Simulation")
	FOnSimulationStateChanged OnSimulationStateChanged;

	/** Broadcast when active orders change */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOrdersUpdated, const TArray<FActiveOrderState>&, Orders);
	UPROPERTY(BlueprintAssignable, Category = "Pickpacker|Orders")
	FOnOrdersUpdated OnOrdersUpdated;

	/** Broadcast when a new order wave starts */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOrderWaveStarted, int32, WaveNumber);
	UPROPERTY(BlueprintAssignable, Category = "Pickpacker|Orders")
	FOnOrderWaveStarted OnOrderWaveStarted;

	/** Broadcast periodic remaining time updates (OrderId -> RemainingSeconds). */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOrderTimesUpdated, const TArray<FOrderTimesUpdatePayload>&, Payload);
	UPROPERTY(BlueprintAssignable, Category = "Pickpacker|Orders")
	FOnOrderTimesUpdated OnOrderTimesUpdated;

	/** Legacy compatibility dispatcher used by existing order UI blueprints. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGlobalTimerCall, float, CurrentTime);
	UPROPERTY(BlueprintAssignable, Category = "Pickpacker|Orders")
	FOnGlobalTimerCall OnGlobalTimerCall;

	/** Broadcast when credits are updated */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCreditsChanged, int32, NewCredits, int32, Delta);
	UPROPERTY(BlueprintAssignable, Category = "Pickpacker|Credits")
	FOnCreditsChanged OnCreditsChanged;

	/** Broadcast when core loop phase changes */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCoreLoopPhaseChanged, ECoreLoopPhase, OldPhase, ECoreLoopPhase, NewPhase);
	UPROPERTY(BlueprintAssignable, Category = "Pickpacker|CoreLoop")
	FOnCoreLoopPhaseChanged OnCoreLoopPhaseChanged;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRunStageUpdated, ERunProgressStage, OldStage, ERunProgressStage, NewStage);
	UPROPERTY(BlueprintAssignable, Category = "Pickpacker|CoreLoop")
	FOnRunStageUpdated OnRunStageUpdated;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMissionDefinitionUpdated, const FMissionDefinition&, MissionDefinition);
	UPROPERTY(BlueprintAssignable, Category = "Pickpacker|Mission")
	FOnMissionDefinitionUpdated OnMissionDefinitionUpdated;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStorageRecordsChanged, const TArray<FStorageRecord>&, StorageRecords);
	UPROPERTY(BlueprintAssignable, Category = "Pickpacker|Storage")
	FOnStorageRecordsChanged OnStorageRecordsChanged;

protected:
	/** Called when level variant is replicated to clients */
	UFUNCTION()
	void OnRep_LevelVariant();

	/** Called when random seed is replicated to clients */
	UFUNCTION()
	void OnRep_RandomSeed();

	/** Called when team suspicion changes */
	UFUNCTION()
	void OnRep_TeamSuspicion();

	/** Called when simulation state changes */
	UFUNCTION()
	void OnRep_SimulationRunning();

	/** Called when active orders are replicated */
	UFUNCTION()
	void OnRep_ActiveOrders();

	UFUNCTION()
	void OnRep_CurrentOrderWaveNumber();

	/** Called when team credits replicate */
	UFUNCTION()
	void OnRep_TeamCredits();

	UFUNCTION()
	void OnRep_OrderTimesPayloads();

	UFUNCTION()
	void OnRep_CoreLoopPhase(ECoreLoopPhase OldPhase);

	UFUNCTION()
	void OnRep_RunStage(ERunProgressStage OldStage);

	UFUNCTION()
	void OnRep_CurrentMissionDefinition();

	UFUNCTION()
	void OnRep_SessionStorageRecords();

	UFUNCTION()
	void OnRep_RunPersonaStats();

	UFUNCTION()
	void OnRep_EndingFlagStates();

	UFUNCTION()
	void OnRep_CurrentRouteSelectionResult();

private:
	/** Replicated level variant data */
	UPROPERTY(ReplicatedUsing = OnRep_LevelVariant)
	UDA_LevelVariant* LevelVariant = nullptr;

	/** Replicated random seed */
	UPROPERTY(ReplicatedUsing = OnRep_RandomSeed)
	int32 RandomSeed = 0;

	/** Replicated team suspicion points */
	UPROPERTY(ReplicatedUsing = OnRep_TeamSuspicion)
	float TeamSuspicion = 0.0f;

	/** Replicated simulation running state */
	UPROPERTY(ReplicatedUsing = OnRep_SimulationRunning)
	bool bSimulationRunning = false;

	/** Replicated team credits */
	UPROPERTY(ReplicatedUsing = OnRep_TeamCredits)
	int32 TeamCredits = 0;

	/** Replicated active orders */
	UPROPERTY(ReplicatedUsing = OnRep_ActiveOrders)
	TArray<FActiveOrderState> ActiveOrders;

	/** Replicated current wave number (1-based, 0 if none) */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentOrderWaveNumber)
	int32 CurrentOrderWaveNumber = 0;

	/** Previous order times payload for change detection */
	UPROPERTY(ReplicatedUsing = OnRep_OrderTimesPayloads)
	TArray<FOrderTimesUpdatePayload> OrderTimesPayloads;

	/** Credit transaction history (server side only) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickpacker|Credits", meta = (AllowPrivateAccess = "true"))
	TArray<FCreditTransaction> CreditHistory;

	/** Maximum stored credit log entries */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickpacker|Credits", meta = (AllowPrivateAccess = "true"))
	int32 MaxStoredCreditTransactions = 20;

	/** Maximum suspicion before alert level increases */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickpacker|Suspicion", meta = (AllowPrivateAccess = "true"))
	float MaxSuspicion = 100.0f;

	/** 택배 드랍 의심 판정 내구도 소모 비율 임계치 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickpacker|Suspicion", meta = (AllowPrivateAccess = "true"))
	float ParcelDropSuspicionDamageRatioThreshold = 0.2f;

	/** Cached anchor subsystem reference */
	UPROPERTY()
	UAnchorRuntimeSubsystem* AnchorSubsystem = nullptr;

	/** 엔딩/월드 진행도 관리 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickpacker|Ending", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEscapeProgressComponent> EscapeProgressComponent;

	/** 열차 이동 관리 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickpacker|Train", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTrainTravelComponent> TrainTravelComponent;

	/** 게임 시간 시스템 변수 */
	
	/** 게임 시작 시간 (World TimeSeconds) */
	UPROPERTY()
	float GameStartTime = 0.0f;

	/** 게임 시간 속도 (1.0 = 정상 속도) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickpacker|Time", meta = (AllowPrivateAccess = "true"))
	float GameTimeSpeed = 1.0f;

	/** 하루 길이 (실제 시간 초 단위, 기본값: 1440초 = 24분) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickpacker|Time", meta = (AllowPrivateAccess = "true"))
	float DayLengthInSeconds = 1440.0f; // 24분 = 하루

	/** Replicated core loop phase */
	UPROPERTY(ReplicatedUsing = OnRep_CoreLoopPhase)
	ECoreLoopPhase CoreLoopPhase = ECoreLoopPhase::None;

	/** Replicated completed trip count */
	UPROPERTY(Replicated)
	int32 CompletedTrips = 0;

	/** Replicated detailed run stage */
	UPROPERTY(ReplicatedUsing = OnRep_RunStage)
	ERunProgressStage RunStage = ERunProgressStage::None;

	/** Replicated mission definition currently driving the run */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentMissionDefinition)
	FMissionDefinition CurrentMissionDefinition;

	/** Replicated run-scoped storage records */
	UPROPERTY(ReplicatedUsing = OnRep_SessionStorageRecords)
	TArray<FStorageRecord> SessionStorageRecords;

	/** Replicated meta persona stats aggregated for the current run */
	UPROPERTY(ReplicatedUsing = OnRep_RunPersonaStats)
	FPersonaStats RunPersonaStats;

	/** Replicated ending flags accumulated during the run */
	UPROPERTY(ReplicatedUsing = OnRep_EndingFlagStates)
	TArray<FEndingFlagState> EndingFlagStates;

	/** Replicated result of the current or last confirmed train route selection */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentRouteSelectionResult)
	FRouteSelectionResult CurrentRouteSelectionResult;

	/** Cached last replicated credits for delta calculations */
	UPROPERTY()
	int32 LastReplicatedTeamCredits = 0;

	/** Timer handle for periodic order time updates */
	FTimerHandle OrderTimesUpdateTimerHandle;

	/** Interval (seconds) for updating remaining times */
	UPROPERTY(EditAnywhere, Category = "Pickpacker|Orders")
	float OrderTimesUpdateInterval = 1.0f;

	/** Start (or ensure) periodic order time updates */
	void EnsureOrderTimesUpdateTimer();

	/** Stop periodic order time updates */
	void StopOrderTimesUpdateTimer();

	/** Gather remaining times and broadcast */
	void BroadcastOrderRemainingTimes();

	void TryApplyRouteRuntime();
};
