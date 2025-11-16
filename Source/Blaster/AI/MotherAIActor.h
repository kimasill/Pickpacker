// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Blaster/GameState/PickpackerGameState.h"
#include "Blaster/AI/DroneActor.h"
#include "MotherAIActor.generated.h"

class UStaticMeshComponent;
class UWidgetComponent;

/**
 * Mother AI State Enumeration
 */
UENUM(BlueprintType)
enum class EMotherAIState : uint8
{
	Normal		UMETA(DisplayName = "Normal"),
	Alert		UMETA(DisplayName = "Alert"),
	Aggressive	UMETA(DisplayName = "Aggressive"),
	RestPeriod	UMETA(DisplayName = "Rest Period")
};

/**
 * Mother AI Actor - Controls the entire surveillance system
 */
UCLASS(BlueprintType, Blueprintable)
class BLASTER_API AMotherAIActor : public AActor
{
	GENERATED_BODY()

public:
	AMotherAIActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Get current AI state
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Mother AI")
	EMotherAIState GetAIState() const { return CurrentState; }

	/**
	 * Set AI state
	 */
	UFUNCTION(BlueprintCallable, Category = "Mother AI")
	void SetAIState(EMotherAIState NewState);

	/**
	 * Get current suspicion level
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Mother AI")
	float GetSuspicionLevel() const;

	/**
	 * Start rest period
	 */
	UFUNCTION(BlueprintCallable, Category = "Mother AI")
	void StartRestPeriod();

	/**
	 * End rest period
	 */
	UFUNCTION(BlueprintCallable, Category = "Mother AI")
	void EndRestPeriod();

	/**
	 * Check if in rest period
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Mother AI")
	bool IsRestPeriod() const { return CurrentState == EMotherAIState::RestPeriod; }

	/**
	 * Spawn drone at location
	 */
	UFUNCTION(BlueprintCallable, Category = "Mother AI")
	ADroneActor* SpawnDrone(const FVector& Location);

	/**
	 * Get all active drones
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Mother AI")
	TArray<ADroneActor*> GetActiveDrones() const { return ActiveDrones; }

	/**
	 * Send warning message to players
	 */
	UFUNCTION(BlueprintCallable, Category = "Mother AI")
	void SendWarning(const FString& Message);

	/**
	 * Punish players (add suspicion, spawn more drones, etc.)
	 */
	UFUNCTION(BlueprintCallable, Category = "Mother AI")
	void PunishPlayers(float SuspicionPoints);

	/**
	 * 플레이어에게 제제 요청 (의심 수치 100 도달 시)
	 */
	UFUNCTION(BlueprintCallable, Category = "Mother AI")
	void RequestPunishment(class ACharacter* Player);

public:
	/** Rest period duration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI|Rest Period")
	float RestPeriodDuration = 60.0f;

	/** Rest period interval */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI|Rest Period")
	float RestPeriodInterval = 300.0f;

	/** Suspicion threshold for alert state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI")
	float AlertSuspicionThreshold = 30.0f;

	/** Suspicion threshold for aggressive state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI")
	float AggressiveSuspicionThreshold = 70.0f;

	/** Maximum number of drones */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI|Drones")
	int32 MaxDrones = 5;

	/** Drone spawn locations */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI|Drones")
	TArray<FVector> DroneSpawnLocations;

	/** Drone class to spawn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI|Drones")
	TSubclassOf<ADroneActor> DroneClass;

	/** Events */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStateChanged, EMotherAIState, NewState);
	UPROPERTY(BlueprintAssignable, Category = "Mother AI|Events")
	FOnStateChanged OnStateChanged;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRestPeriodStarted, float, Duration);
	UPROPERTY(BlueprintAssignable, Category = "Mother AI|Events")
	FOnRestPeriodStarted OnRestPeriodStarted;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRestPeriodEnded);
	UPROPERTY(BlueprintAssignable, Category = "Mother AI|Events")
	FOnRestPeriodEnded OnRestPeriodEnded;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWarningSent, const FString&, Message);
	UPROPERTY(BlueprintAssignable, Category = "Mother AI|Events")
	FOnWarningSent OnWarningSent;

protected:

	UFUNCTION()
	void OnSuspicionChanged_Handler(float NewSuspicionLevel);
	/**
	 * Update AI state based on suspicion
	 */
	UFUNCTION()
	void UpdateAIState();

	/**
	 * Manage drones based on suspicion
	 */
	UFUNCTION()
	void ManageDrones();

	/**
	 * Handle rest period timer
	 */
	UFUNCTION()
	void OnRestPeriodTimerFinished();

	/**
	 * Handle rest period interval timer
	 */
	UFUNCTION()
	void OnRestPeriodIntervalTimerFinished();

private:
	/** Current AI state */
	UPROPERTY(ReplicatedUsing = OnRep_State)
	EMotherAIState CurrentState = EMotherAIState::Normal;

	/** Active drones */
	UPROPERTY(Replicated)
	TArray<TObjectPtr<ADroneActor>> ActiveDrones;

	/** Rest period timer */
	UPROPERTY()
	FTimerHandle RestPeriodTimer;

	/** Rest period interval timer */
	UPROPERTY()
	FTimerHandle RestPeriodIntervalTimer;

	/** Components */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* MotherMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UWidgetComponent* StatusWidget;

	/** Game state reference */
	UPROPERTY()
	APickpackerGameState* GameState;

	/** 플레이어에게 접근 중인지 */
	UPROPERTY(BlueprintReadOnly, Category = "Mother AI", meta = (AllowPrivateAccess = "true"))
	bool bIsApproachingPlayer = false;

	/** 접근 중인 플레이어 */
	UPROPERTY(BlueprintReadOnly, Category = "Mother AI", meta = (AllowPrivateAccess = "true"))
	TWeakObjectPtr<class ACharacter> TargetPlayer;

	/** 접근 속도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI", meta = (AllowPrivateAccess = "true"))
	float ApproachSpeed = 500.0f;

	/** 제제 거리 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI", meta = (AllowPrivateAccess = "true"))
	float PunishmentDistance = 200.0f;

	/** 접근 중 업데이트 */
	void UpdateApproach(float DeltaTime);

	/** 플레이어에게 제제 실행 */
	void ExecutePunishment(class ACharacter* Player);

	/** Replication callback */
	UFUNCTION()
	void OnRep_State(EMotherAIState OldState);
};

