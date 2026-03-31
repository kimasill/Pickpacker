// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeathLocationActor.generated.h"

/**
 * 사망 이동 위치 지정용 액터
 */
UCLASS(BlueprintType, Blueprintable)
class BLASTER_API ADeathLocationActor : public AActor
{
	GENERATED_BODY()

public:
	ADeathLocationActor();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* Root;
};
