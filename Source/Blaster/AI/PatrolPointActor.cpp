// Fill out your copyright notice in the Description page of Project Settings.

#include "PatrolPointActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ArrowComponent.h"
#include "Engine/Engine.h"

APatrolPointActor::APatrolPointActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create visual mesh for editor visibility
	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	RootComponent = VisualMesh;
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	
	// Create direction arrow
	DirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
	DirectionArrow->SetupAttachment(RootComponent);
	DirectionArrow->SetArrowColor(FLinearColor::Green);
	DirectionArrow->SetArrowSize(2.0f);
}

void APatrolPointActor::BeginPlay()
{
	Super::BeginPlay();

	// Update arrow direction if needed
	if (bShouldLookAt && DirectionArrow)
	{
		DirectionArrow->SetWorldRotation(LookAtRotation);
		DirectionArrow->SetVisibility(true);
	}
	else if (DirectionArrow)
	{
		DirectionArrow->SetVisibility(false);
	}
}

#if WITH_EDITOR
void APatrolPointActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Update arrow when properties change in editor
	if (PropertyChangedEvent.Property)
	{
		FName PropertyName = PropertyChangedEvent.Property->GetFName();
		if (PropertyName == GET_MEMBER_NAME_CHECKED(APatrolPointActor, LookAtRotation) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(APatrolPointActor, bShouldLookAt))
		{
			if (DirectionArrow)
			{
				if (bShouldLookAt)
				{
					DirectionArrow->SetWorldRotation(LookAtRotation);
					DirectionArrow->SetVisibility(true);
				}
				else
				{
					DirectionArrow->SetVisibility(false);
				}
			}
		}
	}
}
#endif















