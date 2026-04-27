// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AI/PPAIControllerBase.h"
#include "ModularNPCAIController.generated.h"

/**
 * Lightweight AI controller for modular NPCs that optionally run a combat BT.
 */
UCLASS()
class PICKPACKER_API AModularNPCAIController : public APPAIControllerBase
{
	GENERATED_BODY()

public:
	AModularNPCAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
