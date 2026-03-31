// Fill out your copyright notice in the Description page of Project Settings.

#include "UPPSightPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"

UPPSightPerceptionComponent::UPPSightPerceptionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
}

void UPPSightPerceptionComponent::ApplySightParameters(float SightRadius, float PeripheralVisionAngleDegrees,
	float LoseSightRadiusMultiplier)
{
	if (!SightConfig)
	{
		return;
	}
	SightConfig->SightRadius = SightRadius;
	SightConfig->LoseSightRadius = SightRadius * LoseSightRadiusMultiplier;
	SightConfig->PeripheralVisionAngleDegrees = PeripheralVisionAngleDegrees;
	SightConfig->SetMaxAge(2.0f);
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

	ConfigureSense(*SightConfig);
	SetDominantSense(UAISense_Sight::StaticClass());
}
