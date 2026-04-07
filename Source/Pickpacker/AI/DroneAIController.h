// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AI/PPAIControllerBase.h"
#include "DroneAIController.generated.h"

/**
 * Drone AI Controller - CrowdFollowing + PP 공통 BT 초기화
 */
UCLASS()
class PICKPACKER_API ADroneAIController : public APPAIControllerBase
{
	GENERATED_BODY()

public:
	ADroneAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
