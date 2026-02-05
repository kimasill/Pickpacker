// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Blaster/GameState/PickpackerGameState.h"
#include "Perception/AIPerceptionTypes.h"
#include "PickpackerTypes/PickpackerTypes.h" // for ESuspiciousBehavior
#include "DroneActor.generated.h"

class UBehaviorTree;
class UBehaviorTreeComponent;
class UBlackboardComponent;
class USphereComponent;
class USkeletalMeshComponent;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class ABlasterCharacter; // forward declaration for function signatures

/**
 * Drone State Enumeration
 */
UENUM(BlueprintType)
enum class EDroneState : uint8
{
	Patrol		UMETA(DisplayName = "Patrol"),
	Detecting	UMETA(DisplayName = "Detecting"),
	Chasing		UMETA(DisplayName = "Chasing"),
	Attacking	UMETA(DisplayName = "Attacking"),
	Returning	UMETA(DisplayName = "Returning"),
	Charging	UMETA(DisplayName = "Charging")
};

/**
 * Drone Actor - Surveillance drone that patrols and detects suspicious player behavior
 */
UCLASS(BlueprintType, Blueprintable)
class BLASTER_API ADroneActor : public APawn
{
	GENERATED_BODY()

public:
	ADroneActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Get current drone state
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Drone")
	EDroneState GetDroneState() const { return CurrentState; }

	/**
	 * Set drone state
	 */
	UFUNCTION(BlueprintCallable, Category = "Drone")
	void SetDroneState(EDroneState NewState);

	/**
	 * Get detected player (첫 번째 감지된 플레이어, 호환성 유지)
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Drone")
	class ACharacter* GetDetectedPlayer() const { return DetectedPlayers.Num() > 0 ? DetectedPlayers[0].Get() : nullptr; }

	/**
	 * Get all detected players
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Drone")
	TArray<class ACharacter*> GetDetectedPlayers() const;

	/**
	 * Get suspicion points to add when player is detected
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Drone")
	float GetSuspicionPoints() const { return SuspicionPoints; }

	/**
	 * Start patrol
	 */
	UFUNCTION(BlueprintCallable, Category = "Drone")
	void StartPatrol();

	/**
	 * Stop patrol and return to spawn
	 */
	UFUNCTION(BlueprintCallable, Category = "Drone")
	void StopPatrol();

	/**
	 * Check if drone is active
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Drone")
	bool IsActive() const { return bIsActive; }

	/**
	 * Set drone active state
	 */
	UFUNCTION(BlueprintCallable, Category = "Drone")
	void SetActive(bool bActive);

	/**
 * Get current patrol point
 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Drone|Patrol")
	class APatrolPointActor* GetCurrentPatrolPoint() const;

	/**
	 * Get next patrol point
	 */
	UFUNCTION(BlueprintCallable, Category = "Drone|Patrol")
	class APatrolPointActor* GetNextPatrolPoint();

	/**
	 * Move to next patrol point (increment index)
	 */
	UFUNCTION(BlueprintCallable, Category = "Drone|Patrol")
	void MoveToNextPatrolPoint();

	/**
	 * Check if reached current patrol point
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Drone|Patrol")
	bool HasReachedPatrolPoint(float Tolerance = 100.0f) const;

	/**
	 * Get wait time for current patrol point
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Drone|Patrol")
	float GetCurrentPatrolPointWaitTime() const;

	/**
	 * 플레이어 추격 시작
	 */
	UFUNCTION(BlueprintCallable, Category = "Drone|AI")
	void StartChasing(class ACharacter* Target);

	/**
	 * SuspicionManager 이벤트 핸들러
	 */
	UFUNCTION()
	void OnSuspicionEventReceived(const struct FSuspicionEventData& EventData);

public:
	/** Behavior Tree */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
	UBehaviorTree* BehaviorTree;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	class UFloatingPawnMovement* MovementComponent;

	/** Patrol points (PatrolPointActor instances) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Patrol")
	TArray<class APatrolPointActor*> PatrolPoints;

	/** Current patrol point index */
	UPROPERTY(BlueprintReadOnly, Category = "AI|Patrol")
	int32 CurrentPatrolIndex = 0;

	/** Patrol speed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Patrol")
	float PatrolSpeed = 300.0f;

	/** Detection range */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Detection")
	float DetectionRange = 1000.0f;

	/** Detection angle (degrees) - 전방 120도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Detection")
	float DetectionAngle = 120.0f;

	/** Detection time required */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Detection")
	float DetectionTime = 2.0f;

	/** Chase speed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Chase")
	float ChaseSpeed = 500.0f;

	/** Attack range */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Attack")
	float AttackRange = 200.0f;

	/** Attack damage */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Attack")
	float AttackDamage = 10.0f;

	/** Attack cooldown */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Attack")
	float AttackCooldown = 2.0f;

	/** Suspicion points to add when player is detected */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float SuspicionPoints = 5.0f;

	/** Spawn location (return point) */
	UPROPERTY(BlueprintReadOnly, Category = "AI")
	FVector SpawnLocation;

	/** Charging station location */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Battery")
	AActor* ChargingStation = nullptr;

	/** Maximum battery level (100%) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Battery")
	float MaxBatteryLevel = 100.0f;

	/** Current battery level */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BatteryLevel, Category = "AI|Battery")
	float CurrentBatteryLevel = 100.0f;

	/** Battery consumption rate per second */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Battery")
	float BatteryConsumptionRate = 1.0f; // 1% per second

	/** Battery level threshold to return for charging */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Battery")
	float BatteryLowThreshold = 20.0f; // Return when battery < 20%

	/** Battery charging rate per second */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Battery")
	float BatteryChargingRate = 5.0f; // 5% per second

	/** Weapon component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	class USkeletalMeshComponent* WeaponMesh;

	/** Weapon socket name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Weapon")
	FName WeaponSocketName = FName("WeaponSocket");

	/** Weapon fire range */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Weapon")
	float WeaponFireRange = 500.0f;

	/** Weapon damage */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Weapon")
	float WeaponDamage = 10.0f;

	/** Weapon fire cooldown */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Weapon")
	float WeaponFireCooldown = 1.0f;

	/** Whether to use weapon (generally false, only when attacked or player escaping) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Weapon")
	bool bUseWeapon = false;

	/** Attached object (one at a time) */
	UPROPERTY(BlueprintReadOnly, Category = "AI|Interaction")
	AActor* AttachedObject = nullptr;

	/** Attachment socket name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Interaction")
	FName AttachmentSocketName = FName("AttachmentSocket");

	/** Door interaction range */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Interaction")
	float DoorInteractionRange = 200.0f;

	/** Events */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerDetected, class ACharacter*, Player);
	UPROPERTY(BlueprintAssignable, Category = "Drone|Events")
	FOnPlayerDetected OnPlayerDetected;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerLost, class ACharacter*, Player);
	UPROPERTY(BlueprintAssignable, Category = "Drone|Events")
	FOnPlayerLost OnPlayerLost;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStateChanged, EDroneState, NewState);
	UPROPERTY(BlueprintAssignable, Category = "Drone|Events")
	FOnStateChanged OnStateChanged;

protected:
	UFUNCTION(BlueprintCallable, Category = "Drone|AI")
	void UpdateBlackboard();

	UFUNCTION(BlueprintCallable, Category = "Drone|AI")
	void InitializeAI();

	/**
	 * Detect players in range (from AI Perception)
	 */
	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	/**
	 * Detect players in range (from overlap)
	 */
	UFUNCTION()
	void OnDetectionSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/**
	 * Handle leaving the detection sphere
	 */
	UFUNCTION()
	void OnDetectionSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

		/**
	 * Check if player can be seen (line of sight check)
	 */
	UFUNCTION(BlueprintCallable, Category = "Drone")
	bool CanSeePlayer(class ACharacter* Player) const;

	bool IsCharacterInSight(class ACharacter* Character) const;

	void RefreshDetectedPlayers();

	void RemoveDetectedPlayer(class ACharacter* Character);

    /**
     * Get current battery level (0-100)
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Drone|Battery")
    float GetBatteryLevel() const { return CurrentBatteryLevel; }

	/**
	 * Check if battery is low
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Drone|Battery")
	bool IsBatteryLow() const { return CurrentBatteryLevel <= BatteryLowThreshold; }

	/**
	 * Start charging at charging station
	 */
	UFUNCTION(BlueprintCallable, Category = "Drone|Battery")
	void StartCharging();

	/**
	 * Stop charging
	 */
	UFUNCTION(BlueprintCallable, Category = "Drone|Battery")
	void StopCharging();

	/**
	 * Fire weapon at target
	 */
	UFUNCTION(BlueprintCallable, Category = "Drone|Weapon")
	void FireWeapon(class ACharacter* Target);

	/**
	 * Attach object to drone
	 */
	UFUNCTION(BlueprintCallable, Category = "Drone|Interaction")
	bool AttachObject(AActor* ObjectToAttach);

	/**
	 * Detach object from drone
	 */
	UFUNCTION(BlueprintCallable, Category = "Drone|Interaction")
	void DetachObject();

	/**
	 * Interact with door (open if system-controlled)
	 */
	UFUNCTION(BlueprintCallable, Category = "Drone|Interaction")
	void InteractWithDoor(AActor* Door);

	

	/**
	 * 시야 안의 플레이어들의 의심 행동 상태 지속 체크 (tick에서 호출)
	 */
	void CheckVisiblePlayersSuspiciousBehavior(float DeltaTime);

	/** 의심 행동 체크 간격 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Detection")
	float SuspiciousBehaviorCheckInterval = 0.25f;

	/** 마지막 의심 행동 체크 시간 */
	UPROPERTY()
	float LastSuspiciousBehaviorCheckTime = 0.0f;

	/**
	 * 플레이어의 의심 행동 처리 (공통 로직)
	 * @param BlasterCharacter - 처리할 플레이어
	 * @param Behavior - 의심 행동 타입
	 */
	void ProcessPlayerSuspiciousBehavior(ABlasterCharacter* BlasterCharacter, ESuspiciousBehavior Behavior);

	void AddDetectedPlayer(class ACharacter* Character);

	/** 플레이어별 마지막 의심 행동 처리 시간 (중복 방지용) */
	UPROPERTY()
	TMap<TObjectPtr<ABlasterCharacter>, float> LastProcessedSuspicionTime;

	/** 같은 플레이어의 같은 행동 중복 처리 방지 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Detection")
	float SuspicionProcessCooldown = 2.0f;

	/**
	 * Add suspicion to game state
	 */
	UFUNCTION()
	void AddSuspicion(float Points);

	/**
	 * Attack player
	 */
	UFUNCTION()
	void AttackPlayer(class ACharacter* Player);

	/**
	 * Handle attack cooldown
	 */
	UFUNCTION()
	void OnAttackCooldownFinished();

	UFUNCTION()
	void DrawPerceptionDebug();

private:
	/** Current state */
	UPROPERTY(ReplicatedUsing = OnRep_State)
	EDroneState CurrentState = EDroneState::Patrol;

	/** Detected players (여러 명 감지 가능) */
	UPROPERTY(Replicated)
	TArray<TWeakObjectPtr<class ACharacter>> DetectedPlayers;

	UPROPERTY()
	TArray<TWeakObjectPtr<class ACharacter>> OverlappedPlayers;

	/** Detection timer */
	UPROPERTY()
	float CurrentDetectionTime = 0.0f;

	/** Attack timer */
	UPROPERTY()
	float CurrentAttackCooldown = 0.0f;

	/** Battery timer */
	UPROPERTY()
	float BatteryTimer = 0.0f;

	/** Whether currently charging */
	UPROPERTY(Replicated)
	bool bIsCharging = false;

	/** Weapon fire timer */
	UPROPERTY()
	float WeaponFireTimer = 0.0f;

	/** Whether weapon can fire */
	UPROPERTY()
	bool bCanFireWeapon = true;

	/** Whether drone is active */
	UPROPERTY(Replicated)
	bool bIsActive = true;

	/** Components */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USphereComponent* RootSphereComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* DroneMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USphereComponent* DetectionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UAIPerceptionComponent* PerceptionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UAISenseConfig_Sight* SightConfig;

	/** Replication callback */
	UFUNCTION()
	void OnRep_State(EDroneState OldState);

	/** Battery level replication callback */
	UFUNCTION()
	void OnRep_BatteryLevel(float OldBatteryLevel);

	/** Game state reference */
	UPROPERTY()
	APickpackerGameState* GameState;
};

