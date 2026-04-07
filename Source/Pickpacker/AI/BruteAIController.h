// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AI/PPAIControllerBase.h"
#include "BruteAIController.generated.h"

/**
 * Brute AI Controller - CrowdFollowing; BT는 추후 Brute 트리에서 사용
 */
UCLASS()
class PICKPACKER_API ABruteAIController : public APPAIControllerBase
{
	GENERATED_BODY()

public:
	ABruteAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
