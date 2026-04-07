// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PPPatrolRouteComponent.generated.h"

class APatrolPointActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPPPatrolRouteChanged);

/**
 * 웨이포인트 기반 순찰 경로 — Drone 등에서 재사용.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PICKPACKER_API UPPPatrolRouteComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPPPatrolRouteComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol")
	TArray<TObjectPtr<APatrolPointActor>> PatrolPoints;

	UPROPERTY(BlueprintReadOnly, Category = "Patrol")
	int32 CurrentPatrolIndex = 0;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Patrol")
	APatrolPointActor* GetCurrentPatrolPoint() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Patrol")
	APatrolPointActor* GetNextPatrolPoint() const;

	UFUNCTION(BlueprintCallable, Category = "Patrol")
	void MoveToNextPatrolPoint();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Patrol")
	bool HasReachedPatrolPoint(float Tolerance = 100.0f) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Patrol")
	float GetCurrentPatrolPointWaitTime() const;

	UFUNCTION(BlueprintCallable, Category = "Patrol")
	void ResetToStart();

	UPROPERTY(BlueprintAssignable, Category = "Patrol")
	FOnPPPatrolRouteChanged OnPatrolRouteChanged;

	static UPPPatrolRouteComponent* FindPatrolRoute(AActor* Owner);
};
