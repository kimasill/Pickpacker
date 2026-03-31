// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_PPGeneratePatrolPointInBounds.generated.h"

/**
 * Blackboard의 Volume/Inspection 액터 바운드 내 랜덤 패트롤 포인트 생성 (Mother 전용 캐스팅 없음).
 */
UCLASS()
class BLASTER_API UBTTask_PPGeneratePatrolPointInBounds : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_PPGeneratePatrolPointInBounds();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector PatrolTargetLocationKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector VolumeActorKey;

	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0.0"))
	float MinPatrolRadius = 50.0f;

	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0.0"))
	float MaxPatrolRadius = 500.0f;
};
