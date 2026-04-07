// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Perception/AIPerceptionComponent.h"
#include "UPPSightPerceptionComponent.generated.h"

class UAISenseConfig_Sight;

/**
 * 시야 기반 AI Perception 공통 설정 (Mother / Brute / Drone 등).
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PICKPACKER_API UPPSightPerceptionComponent : public UAIPerceptionComponent
{
	GENERATED_BODY()

public:
	UPPSightPerceptionComponent(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "AI|Perception")
	void ApplySightParameters(float SightRadius, float PeripheralVisionAngleDegrees, float LoseSightRadiusMultiplier = 1.2f);

	UFUNCTION(BlueprintPure, Category = "AI|Perception")
	UAISenseConfig_Sight* GetSightConfig() const { return SightConfig; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "AI|Perception")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;
};
