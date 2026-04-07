// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_MotherPatrolInVolume.generated.h"

/**
 * Node Memory for Patrol In Volume Task
 */
USTRUCT()
struct FMotherPatrolInVolumeMemory
{
	GENERATED_BODY()

	FVector CurrentPatrolTarget;
	float ElapsedTime;
	float WaitTime;
	bool bIsActive;

	FMotherPatrolInVolumeMemory()
		: CurrentPatrolTarget(FVector::ZeroVector)
		, ElapsedTime(0.0f)
		, WaitTime(0.0f)
		, bIsActive(false)
	{
	}
};

/**
 * Patrol within an inspection volume bounds (for Mother AI)
 * 
 * @deprecated This task has been replaced with a more modular approach:
 * - Use BTService_MotherGeneratePatrolPoint to generate random patrol points
 * - Use BTTask_MotherMoveToLocation to move to each point
 * - Use BTTask_MotherCheckPatrolComplete to check completion
 * 
 * The new approach is more stable and allows better control in Behavior Tree sequences.
 * See PATROL_REFACTOR_GUIDE.md for migration instructions.
 */
UCLASS()
class PICKPACKER_API UBTTask_MotherPatrolInVolume : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_MotherPatrolInVolume();

	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FMotherPatrolInVolumeMemory); }
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	/** Inspection actor key in blackboard */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector InspectionActorKey;

	/** Inspection location key in blackboard */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector InspectionLocationKey;

	/** Patrol duration in seconds */
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0.0"))
	float PatrolDuration = 30.0f;

	/** Movement speed during patrol */
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0.0"))
	float MovementSpeed = 300.0f;

	/** Distance to consider reached patrol point */
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0.0"))
	float AcceptableRadius = 100.0f;

	/** Time to wait at each patrol point before moving to next */
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0.0"))
	float WaitAtPointDuration = 2.0f;

	/** Minimum distance from center to generate patrol point */
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0.0"))
	float MinPatrolRadius = 50.0f;

	/** Maximum distance from center to generate patrol point */
	UPROPERTY(EditAnywhere, Category = "Patrol", meta = (ClampMin = "0.0"))
	float MaxPatrolRadius = 500.0f;

private:
	/** Get bounds from inspection actor */
	bool GetInspectionVolumeBounds(AActor* InspectionActor, FVector& OutCenter, FVector& OutExtent) const;

	/** Generate random patrol point within bounds */
	FVector GenerateRandomPatrolPoint(const FVector& Center, const FVector& Extent) const;
};
