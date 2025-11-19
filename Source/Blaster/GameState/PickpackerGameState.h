// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Blaster/PickpackerTypes/PickpackerTypes.h"
#include "Blaster/DataAssets/DA_LevelVariant.h"
#include "PickpackerGameState.generated.h"

class UAnchorRuntimeSubsystem;

/**
 * Pickpacker Game State - Manages warehouse simulation state
 * Tracks suspicion levels, team performance, and game progression
 */
UCLASS()
class BLASTER_API APickpackerGameState : public AGameState
{
	GENERATED_BODY()

public:
	APickpackerGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

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

public:
	/** Broadcast when suspicion level changes */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSuspicionChanged, float, NewSuspicionLevel);
	UPROPERTY(BlueprintAssignable, Category = "Pickpacker|Suspicion")
	FOnSuspicionChanged OnSuspicionChanged;

	/** Broadcast when simulation state changes */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSimulationStateChanged, bool, bIsRunning);
	UPROPERTY(BlueprintAssignable, Category = "Pickpacker|Simulation")
	FOnSimulationStateChanged OnSimulationStateChanged;

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

	/** Maximum suspicion before alert level increases */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickpacker|Suspicion", meta = (AllowPrivateAccess = "true"))
	float MaxSuspicion = 100.0f;

	/** Cached anchor subsystem reference */
	UPROPERTY()
	UAnchorRuntimeSubsystem* AnchorSubsystem = nullptr;

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
};