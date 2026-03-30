// Fill out your copyright notice in the Description page of Project Settings.

#include "PPPatrolRouteComponent.h"
#include "Blaster/AI/PatrolPointActor.h"

UPPPatrolRouteComponent::UPPPatrolRouteComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

APatrolPointActor* UPPPatrolRouteComponent::GetCurrentPatrolPoint() const
{
	if (PatrolPoints.IsValidIndex(CurrentPatrolIndex))
	{
		return PatrolPoints[CurrentPatrolIndex];
	}
	return nullptr;
}

APatrolPointActor* UPPPatrolRouteComponent::GetNextPatrolPoint() const
{
	if (PatrolPoints.Num() == 0)
	{
		return nullptr;
	}
	const int32 NextIndex = (CurrentPatrolIndex + 1) % PatrolPoints.Num();
	return PatrolPoints.IsValidIndex(NextIndex) ? PatrolPoints[NextIndex] : nullptr;
}

void UPPPatrolRouteComponent::MoveToNextPatrolPoint()
{
	if (PatrolPoints.Num() == 0)
	{
		return;
	}
	CurrentPatrolIndex = (CurrentPatrolIndex + 1) % PatrolPoints.Num();
	OnPatrolRouteChanged.Broadcast();
}

bool UPPPatrolRouteComponent::HasReachedPatrolPoint(float Tolerance) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}
	APatrolPointActor* CurrentPoint = GetCurrentPatrolPoint();
	if (!CurrentPoint)
	{
		return false;
	}
	const float Distance = FVector::Dist(Owner->GetActorLocation(), CurrentPoint->GetActorLocation());
	return Distance <= Tolerance;
}

float UPPPatrolRouteComponent::GetCurrentPatrolPointWaitTime() const
{
	if (APatrolPointActor* CurrentPoint = GetCurrentPatrolPoint())
	{
		return CurrentPoint->WaitTime;
	}
	return 2.0f;
}

void UPPPatrolRouteComponent::ResetToStart()
{
	CurrentPatrolIndex = 0;
	OnPatrolRouteChanged.Broadcast();
}

UPPPatrolRouteComponent* UPPPatrolRouteComponent::FindPatrolRoute(AActor* Owner)
{
	if (!Owner)
	{
		return nullptr;
	}
	return Owner->FindComponentByClass<UPPPatrolRouteComponent>();
}
