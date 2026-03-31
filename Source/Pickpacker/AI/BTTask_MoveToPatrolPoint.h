// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_MoveToPatrolPoint.generated.h"

/**
 * Node Memory for Move To Patrol Point Task
 */
USTRUCT()
struct FMoveToPatrolPointMemory
{
	GENERATED_BODY()

	FVector TargetLocation;
	bool bIsActive;
	
	// PID/Steering smoothing을 위한 변수들
	FVector SmoothedDirection;
	FVector PreviousDirection;
	float DirectionChangeRate;

	FMoveToPatrolPointMemory()
		: TargetLocation(FVector::ZeroVector)
		, bIsActive(false)
		, SmoothedDirection(FVector::ZeroVector)
		, PreviousDirection(FVector::ZeroVector)
		, DirectionChangeRate(0.0f)
	{
	}
};

/**
 * Custom Move To Task for Drone using Floating Pawn Movement
 */
UCLASS()
class BLASTER_API UBTTask_MoveToPatrolPoint : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_MoveToPatrolPoint();

	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FMoveToPatrolPointMemory); }
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;	
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	/** Blackboard key for patrol point location */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector PatrolPointLocationKey;

	/** Acceptable radius to consider reached */
	UPROPERTY(EditAnywhere, Category = "Movement")
	float AcceptableRadius = 150.0f;

	/** Movement speed multiplier */
	UPROPERTY(EditAnywhere, Category = "Movement")
	float SpeedMultiplier = 1.0f;

	/** 회피 시스템: 샘플링 각도 수 */
	UPROPERTY(EditAnywhere, Category = "Avoidance")
	int32 AvoidanceSampleCount = 8;

	/** 회피 시스템: 샘플링 각도 범위 (도) */
	UPROPERTY(EditAnywhere, Category = "Avoidance")
	float AvoidanceSampleAngleRange = 90.0f;

	/** 회피 시스템: 스캔 거리 */
	UPROPERTY(EditAnywhere, Category = "Avoidance")
	float AvoidanceScanDistance = 300.0f;

	/** 회피 시스템: 최소 안전 거리 */
	UPROPERTY(EditAnywhere, Category = "Avoidance")
	float AvoidanceMinSafeDistance = 100.0f;

	/** Steering smoothing: 방향 변경 속도 */
	UPROPERTY(EditAnywhere, Category = "Steering")
	float SteeringSmoothingRate = 5.0f;

	/** Steering smoothing: 최대 방향 변경 속도 */
	UPROPERTY(EditAnywhere, Category = "Steering")
	float MaxDirectionChangeRate = 180.0f;

	/** 상승값 가중치: 기본 상승값 */
	UPROPERTY(EditAnywhere, Category = "Avoidance")
	float BaseLiftValue = 0.3f;

	/** 상승값 가중치: 최대 상승값 */
	UPROPERTY(EditAnywhere, Category = "Avoidance")
	float MaxLiftValue = 1.0f;

	/** 상승값 가중치: 가중치 곡선 지수 */
	UPROPERTY(EditAnywhere, Category = "Avoidance")
	float LiftWeightExponent = 2.0f;
};

