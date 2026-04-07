// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameState/PickpackerGameState.h"
#include "AI/DroneActor.h"
#include "Perception/AIPerceptionTypes.h"
#include "MotherAIActor.generated.h"

class USkeletalMeshComponent;
class UWidgetComponent;
class UPPSightPerceptionComponent;
class UMotherGameplayComponent;

/**
 * Mother AI State Enumeration
 */
UENUM(BlueprintType)
enum class EMotherAIState : uint8
{
	Normal			UMETA(DisplayName = "Normal"),
	Alert			UMETA(DisplayName = "Alert"),
	Aggressive		UMETA(DisplayName = "Aggressive"),
	RestPeriod		UMETA(DisplayName = "Rest Period"),
	AtControlTower	UMETA(DisplayName = "At Control Tower"),
	Inspecting		UMETA(DisplayName = "Inspecting Facilities"),
	ChasingPlayer	UMETA(DisplayName = "Chasing Player"),
	Searching		UMETA(DisplayName = "Searching - Lost Player During Chase")
};

/**
 * Mother AI Actor - Controls the entire surveillance system
 * Uses Behavior Tree for AI logic
 */
UCLASS(BlueprintType, Blueprintable)
class PICKPACKER_API AMotherAIActor : public ACharacter
{
	GENERATED_BODY()

	friend class UMotherGameplayComponent;

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

	/**
	 * SuspicionManager 이벤트 핸들러 (즉시 처벌)
	 */
	UFUNCTION()
	void OnSuspicionEventReceived(const struct FSuspicionEventData& EventData);

	/**
	 * 시설 점검 트리거 (Blackboard 업데이트, 비헤이비어 트리가 이동 처리)
	 */
	UFUNCTION(BlueprintCallable, Category = "Mother AI")
	void TriggerInspection();

	/**
	 * 시설 점검 시작 (비헤이비어 트리에서 위치 도착 후 호출)
	 */
	UFUNCTION(BlueprintCallable, Category = "Mother AI")
	void StartInspection();

	/**
	 * 시설 점검 완료
	 */
	UFUNCTION(BlueprintCallable, Category = "Mother AI")
	void CompleteInspection();

	/**
	 * 통제 타워로 복귀
	 */
	UFUNCTION(BlueprintCallable, Category = "Mother AI")
	void ReturnToControlTower();

	/**
	 * Getter functions for Behavior Tree
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Mother AI")
	TArray<FVector> GetInspectionLocations() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Mother AI")
	FVector GetCurrentInspectionLocation() const { return CurrentInspectionLocation; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Mother AI")
	float GetInspectionDuration() const { return InspectionDuration; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Mother AI")
	FVector GetControlTowerLocation() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Mother AI")
	AActor* GetControlTowerActor() const { return ControlTowerActor; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Mother AI")
	const TArray<AActor*>& GetInspectionActorLocations() const { return InspectionActorLocations; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Mother AI|Animation")
	UAnimMontage* GetPunishmentMontage() const { return PunishmentMontage; }

	/** 점검 몽타주 가져오기 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Mother AI|Animation")
	UAnimMontage* GetInspectionMontage() const { return InspectionMontage; }

	/** Get punishment distance */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Mother AI")
	float GetPunishmentDistance() const { return PunishmentDistance; }

	/** Update player location from drone report */
	UFUNCTION(BlueprintCallable, Category = "Mother AI")
	void UpdatePlayerLocationFromDrone(class ACharacter* Player, const FVector& NewLocation);

	/** 특정 위치로 이동 (유틸리티 함수) */
	void MoveToLocation(const FVector& TargetLocation, float Speed);

	/** 플레이어를 볼 수 있는지 확인 */
	bool CanSeePlayer(class ACharacter* Player) const;

	/** 현재 위치가 목표 위치에 도달했는지 확인 */
	bool HasReachedLocation(const FVector& TargetLocation, float Tolerance = 100.0f) const;

	/** Get all detected players */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Mother AI|Detection")
	TArray<class ACharacter*> GetDetectedPlayers() const;


	/** 플레이어에게 제제 실행 */
	void ExecutePunishment(class ACharacter* Player);
	UFUNCTION(BlueprintCallable)
	void OnPunishmentHit();

	UFUNCTION(BlueprintCallable)
	void OnPunishmentEnd(UAnimMontage* Montage, bool bInterrupted);

	/** 처벌 강제 종료 (플레이어 사망 등) */
	UFUNCTION(BlueprintCallable)
	void StopPunishmentForTarget(class ACharacter* Player);

	UFUNCTION(BlueprintCallable)
	void OnInspectionEnd();
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

	/** 중앙 통제 타워 액터 (위치는 액터의 위치에서 가져옴) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI|Control Tower")
	AActor* ControlTowerActor = nullptr;

	/** 통제 타워 폴백 태그: ControlTowerActor가 null일 때 이 태그로 월드에서 액터를 찾음 (패키징/SeamlessTravel용) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI|Control Tower", meta = (DisplayName = "Control Tower Tag (Fallback)"))
	FName ControlTowerTag;

	/** 시설 점검 액터 목록 (위치는 각 액터의 위치에서 가져옴) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI|Inspection")
	TArray<AActor*> InspectionActorLocations;

	/** 점검 액터 폴백 태그: InspectionActorLocations가 null일 때 이 태그로 월드에서 액터를 찾음 (패키징 빌드용) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI|Inspection", meta = (DisplayName = "Inspection Actor Tag (Fallback)"))
	FName InspectionActorTag;

	/** 시설 점검 시간 (게임시간 기준, 하루에 두번) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI|Inspection")
	TArray<float> InspectionTimes = { 6.0f, 18.0f }; // 오전 6시, 오후 6시

	/** 시설 점검 소요 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI|Inspection")
	float InspectionDuration = 30.0f;

	/** 이동 속도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI|Movement")
	float MovementSpeed = 400.0f;

	/** 감지 범위 (플레이어 의심 행위 감지) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI|Detection")
	float DetectionRange = 2000.0f;

	/** 감지 각도 (도) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI|Detection")
	float DetectionAngle = 120.0f;

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

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPunishmentFinished, bool, bInterrupted);
	UPROPERTY(BlueprintAssignable, Category = "Mother AI|Events")
	FOnPunishmentFinished OnPunishmentFinished;

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

	/**
	 * Detect players in range (from AI Perception)
	 */
	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	/**
	 * 모든 플레이어 상태에 구독 (의심 수치 100 도달 감지용)
	 */
	UFUNCTION()
	void SubscribeToAllPlayerStates();

	/**
	 * 플레이어 의심 수치 변경 핸들러
	 */
	UFUNCTION()
	void OnPlayerSuspicionChanged(float NewSuspicion, float OldSuspicion);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Punishment();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PunishmentEnd(bool interrupted);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Inspection();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_InspectionEnd(bool interrupted);
	
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

	/** Punishment animation montage */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI|Animation", meta = (
		AllowPrivateAccess = "true"))
	UAnimMontage* PunishmentMontage;
	/** Inspection animation montage */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI|Animation", meta = (
		AllowPrivateAccess = "true"))
	UAnimMontage* InspectionMontage;

	/** Components */
	// Note: Character는 기본적으로 GetMesh()로 SkeletalMeshComponent를 제공합니다.
	// 필요시 블루프린트에서 SkeletalMesh를 설정하세요.

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UWidgetComponent* StatusWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UPPSightPerceptionComponent* PerceptionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UMotherGameplayComponent* MotherGameplay;

	/** Detected players (여러 명 감지 가능) */
	UPROPERTY(Replicated)
	TArray<TWeakObjectPtr<class ACharacter>> DetectedPlayers;

	/** Game state reference */
	UPROPERTY()
	APickpackerGameState* GameState;

	/** 플레이어 상태 구독 타이머 핸들 */
	UPROPERTY()
	FTimerHandle SubscribeToPlayersTimerHandle;

	/** 구독 중인 플레이어 상태 목록 (중복 구독 방지) */
	UPROPERTY()
	TSet<TObjectPtr<class ABlasterPlayerState>> SubscribedPlayerStates;

	/** 플레이어에게 접근 중인지 */
	UPROPERTY(BlueprintReadOnly, Category = "Mother AI", meta = (AllowPrivateAccess = "true"))
	bool bIsApproachingPlayer = false;

	/** 처벌 실행 중인지 */
	UPROPERTY(BlueprintReadOnly, Category = "Mother AI", meta = (AllowPrivateAccess = "true"))
	bool bIsExecutingPunishment = false;

	UPROPERTY(BlueprintReadOnly, Category = "Mother AI", meta = (AllowPrivateAccess = "true"))
	bool bIsInspecting = false;

	/** 접근 중인 플레이어 */
	UPROPERTY(BlueprintReadOnly, Category = "Mother AI", meta = (AllowPrivateAccess = "true"))
	TWeakObjectPtr<class ACharacter> TargetPlayer;

	/** 접근 속도 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI", meta = (AllowPrivateAccess = "true"))
	float ApproachSpeed = 500.0f;

	/** 제제 거리 (모션을 잘 볼 수 있는 거리) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI", meta = (AllowPrivateAccess = "true"))
	float PunishmentDistance = 200.0f;

	/** 카메라 회전 대기 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI", meta = (AllowPrivateAccess = "true"))
	float CameraRotationWaitTime = 1.5f;

	/** Replication callback */
	UFUNCTION()
	void OnRep_State(EMotherAIState OldState);

	UFUNCTION(BlueprintCallable, Category = "Mother AI|Animation")
	void PlayPunishmentMontage();

	UFUNCTION(BlueprintCallable, Category = "Mother AI|Animation")
	void PlayInspectionMontage();

	/** 카메라 회전 완료 후 처벌 모션 시작 */
	void StartPunishmentMontage();

	/** 이동 중 정면 문 감지 및 인터랙트 */
	void TryOpenDoorAhead();
	

private:
	/** 현재 시설 점검 인덱스 */
	UPROPERTY()
	int32 CurrentInspectionIndex = 0;

	/** 현재 점검 중인 시설 위치 */
	UPROPERTY()
	FVector CurrentInspectionLocation = FVector::ZeroVector;

	/** 현재 점검 중인 시설 액터 */
	UPROPERTY()
	AActor* CurrentInspectionActor = nullptr;

	/** 점검 시작 시간 (월드 초 단위) */
	UPROPERTY()
	float InspectionStartTime = 0.0f;

public:
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Mother AI|Inspection")
	AActor* GetCurrentInspectionActor() const { return CurrentInspectionActor; }

	/** 다음 점검 시간 계산 */
	void ScheduleNextInspection();

	/** Blackboard 초기 TargetLocation 설정 (패키징 빌드 대비) */
	void EnsureBlackboardInitialized();

	/** BT/BB 초기화 재시도 (패키징 빌드에서 OnPossess 지연 시) */
	void EnsureBehaviorTreeInitialized();

	/** BP 참조가 null일 때 태그로 ControlTowerActor, InspectionActorLocations 갱신 (패키징/SeamlessTravel용) */
	void RefreshReferencesFromTags();

	/** 점검 타이머 핸들 */
	UPROPERTY()
	FTimerHandle InspectionTimerHandle;

	/** ScheduleNextInspection 재시도 타이머 (GameState 미준비 시) */
	UPROPERTY()
	FTimerHandle ScheduleRetryTimerHandle;

	/** BT 초기화 재시도 타이머 (패키징 빌드 대비) */
	UPROPERTY()
	FTimerHandle BTInitRetryTimerHandle;

	/** 태그 기반 참조 갱신 지연 타이머 */
	UPROPERTY()
	FTimerHandle RefreshReferencesTimerHandle;

	/** 처벌 모션 실행 전 카메라 회전 대기 타이머 */
	UPROPERTY()
	FTimerHandle PunishmentCameraRotationTimer;

	/** 플레이어 감지 체크 타이머 */
	UPROPERTY()
	float LastDetectionCheckTime = 0.0f;

	/** 감지 체크 간격 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI|Detection", meta = (AllowPrivateAccess = "true"))
	float DetectionCheckInterval = 0.5f;

	// 추격/걷기 속도 설정
	UPROPERTY(EditDefaultsOnly, Category="Movement")
	float WalkSpeed = 300.f;

	UPROPERTY(EditDefaultsOnly, Category="Movement")
	float ChaseSpeed = 600.f;

	// 현재 적용된 속도를 추적(옵션)
	UPROPERTY(VisibleInstanceOnly, Category="Movement")
	float CurrentDesiredSpeed = 300.f;

	// 상태에 따른 속도 적용 헬퍼
	void ApplySpeedForState(EMotherAIState NewState);

	FVector GetAvoidanceDirection(const FVector& DesiredDirection) const;

	/** 장애물 회피 체크 거리 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI|Movement", meta = (AllowPrivateAccess = "true"))
	float AvoidanceCheckDistance = 150.0f;

	/** 장애물 회피 스윕 반경 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI|Movement", meta = (AllowPrivateAccess = "true"))
	float AvoidanceCheckRadius = 35.0f;

	/** 장애물 회피 각도 (도) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI|Movement", meta = (AllowPrivateAccess = "true"))
	float AvoidanceAngleDegrees = 45.0f;

	/** 문 태그 (문 액터에 설정) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI|Door", meta = (AllowPrivateAccess = "true"))
	FName DoorActorTag = TEXT("Door");

	/** 문 감지 거리 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI|Door", meta = (AllowPrivateAccess = "true"))
	float DoorCheckDistance = 160.0f;

	/** 문 감지 반경 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI|Door", meta = (AllowPrivateAccess = "true"))
	float DoorCheckRadius = 40.0f;

	/** 문 인터랙트 쿨다운 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI|Door", meta = (AllowPrivateAccess = "true"))
	float DoorInteractCooldown = 0.5f;

	/** 마지막 문 인터랙트 시간 */
	UPROPERTY()
	float LastDoorInteractTime = -1000.0f;

	/** 마지막으로 인터랙트한 문 */
	UPROPERTY()
	TWeakObjectPtr<AActor> LastDoorInteracted;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother AI|Inspection")
	int32 InspectionCounter = 5;
};

