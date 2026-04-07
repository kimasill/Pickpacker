// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MotherGameplayComponent.generated.h"

class ADroneActor;
class AMotherAIActor;

/**
 * Mother 전용: 드론 스폰, 팀 의심, 드론 수 관리, 점검 스케줄.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PICKPACKER_API UMotherGameplayComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMotherGameplayComponent();

	ADroneActor* SpawnDroneAt(const FVector& Location);

	void PunishPlayers(float SuspicionPoints);

	void ManageDrones();

	void ScheduleNextInspection();

protected:
	AMotherAIActor* GetMother() const;
};
