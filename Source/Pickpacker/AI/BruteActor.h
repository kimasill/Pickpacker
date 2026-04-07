// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Perception/AIPerceptionTypes.h"
#include "BruteActor.generated.h"

class UPPSightPerceptionComponent;
class UBehaviorTree;
class UBlackboardData;
class AMotherPatrolZoneActor;
class ABlasterCharacter;

UENUM(BlueprintType)
enum class EBruteRole : uint8
{
	Guard	UMETA(DisplayName = "Guard"),
	Watch	UMETA(DisplayName = "Watch")
};

UENUM(BlueprintType)
enum class EBruteState : uint8
{
	Idle		UMETA(DisplayName = "Idle"),
	Patrolling	UMETA(DisplayName = "Patrolling"),
	Chasing		UMETA(DisplayName = "Chasing"),
	Suspicious	UMETA(DisplayName = "Suspicious"),
	Executing	UMETA(DisplayName = "Executing"),
	Returning	UMETA(DisplayName = "Returning")
};

/**
 * Brute AI Actor
 * - Role: Guard(자리 고정/저지) or Watch(구역 순찰/발견 시 추격-처형)
 * - Behavior: Sight detection -> chase -> execute montage when close
 * - If target lost/out of range: suspicious for a while -> return to post/patrol
 */
UCLASS(BlueprintType, Blueprintable)
class PICKPACKER_API ABruteActor : public ACharacter
{
	GENERATED_BODY()

public:
	ABruteActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// --- State/Role ---
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Brute")
	EBruteRole GetRole() const { return BruteRole; }

	UFUNCTION(BlueprintCallable, Category="Brute")
	void SetRole(EBruteRole NewRole);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Brute")
	EBruteState GetState() const { return State; }

	UFUNCTION(BlueprintCallable, Category="Brute")
	void SetState(EBruteState NewState);

	// --- Execution (animation notify hooks) ---
	UFUNCTION(BlueprintCallable, Category="Brute|Execution")
	void ExecuteTarget(ABlasterCharacter* Target);

	/** AnimNotify에서 호출 (타겟에게 처형 판정 적용) */
	UFUNCTION(BlueprintCallable, Category="Brute|Execution")
	void OnExecutionHit();

	UFUNCTION(BlueprintCallable, Category="Brute|Execution")
	void OnExecutionEnd(UAnimMontage* Montage, bool bInterrupted);

	/** 처형 강제 종료 (타겟 사망/이탈 등) */
	UFUNCTION(BlueprintCallable, Category="Brute|Execution")
	void StopExecutionForTarget(ABlasterCharacter* Target);

	// --- Tuning ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Brute|Role")
	EBruteRole BruteRole = EBruteRole::Guard;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Brute|Detection")
	float DetectionRange = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Brute|Detection")
	float DetectionAngle = 120.0f;

	/** 타겟을 놓쳤을 때 의심 상태로 머무는 시간 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Brute|Suspicion")
	float SuspicionDuration = 4.0f;

	/** 이 거리 이상 멀어지면(또는 시야 상실) 의심 상태로 전환 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Brute|Chase")
	float MaxChaseDistance = 3500.0f;

	/** 가드 역할일 때, 근무 위치에서 이 거리 이상 벗어나면 추격 중단 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Brute|Guard")
	float MaxGuardPursuitDistance = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Brute|Execution")
	float ExecutionDistance = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Brute|Execution")
	float CameraRotationWaitTime = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Brute|Movement")
	float WalkSpeed = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Brute|Movement")
	float ChaseSpeed = 600.f;

	/** 감시(Watch) 역할일 때 순찰 구역 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Brute|Patrol")
	AMotherPatrolZoneActor* PatrolZone = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Brute|Patrol")
	float PatrolAcceptanceRadius = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Brute|Patrol")
	float PatrolWaitTime = 1.0f;

	/** null이면 기존 Tick FSM. 설정 시 BT가 주행(트리 에셋에서 구성). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Brute|AI")
	UBehaviorTree* BruteBehaviorTree = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Brute|AI")
	UBlackboardData* BruteBlackboardAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Brute|Animation", meta=(AllowPrivateAccess="true"))
	UAnimMontage* ExecutionMontage = nullptr;

	// --- Events ---
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBruteStateChanged, EBruteState, NewState);
	UPROPERTY(BlueprintAssignable, Category="Brute|Events")
	FOnBruteStateChanged OnStateChanged;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnExecutionFinished, bool, bInterrupted);
	UPROPERTY(BlueprintAssignable, Category="Brute|Events")
	FOnExecutionFinished OnExecutionFinished;

protected:
	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	UFUNCTION()
	void TryStartBruteBehaviorTree();

	bool CanSeePlayer(ABlasterCharacter* Player) const;
	void StartChase(ABlasterCharacter* Target);
	void EnterSuspiciousState();
	void ClearSuspicionAndReturn();
	void StartPatrolIfNeeded();
	void MoveToLocation(const FVector& TargetLocation, float AcceptanceRadius);
	void ApplySpeedForState(EBruteState NewState);
	FVector GenerateRandomPatrolPoint() const;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayExecution();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ExecutionEnd(bool bInterrupted);

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	UPPSightPerceptionComponent* PerceptionComp;

	UPROPERTY(ReplicatedUsing=OnRep_State)
	EBruteState State = EBruteState::Idle;

	UFUNCTION()
	void OnRep_State(EBruteState OldState);

	UPROPERTY()
	FVector GuardLocation = FVector::ZeroVector;

	UPROPERTY()
	FVector LastKnownTargetLocation = FVector::ZeroVector;

	UPROPERTY()
	FTimerHandle SuspicionTimerHandle;

	UPROPERTY()
	FTimerHandle ExecutionCameraRotationTimer;

	UPROPERTY()
	bool bCanSeeTarget = false;

	UPROPERTY()
	bool bIsExecuting = false;

	UPROPERTY()
	TWeakObjectPtr<ABlasterCharacter> TargetPlayer;

	UPROPERTY()
	float NextPatrolMoveTime = 0.0f;
};

