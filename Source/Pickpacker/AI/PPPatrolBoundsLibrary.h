// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 볼륨/액터 바운드에서 패트롤 포인트 샘플링 (Mother 점검, 일반 BT 등).
 */
namespace PPPatrolBoundsLibrary
{
	BLASTER_API bool GetVolumeBoundsFromActor(AActor* VolumeActor, FVector& OutCenter, FVector& OutExtent);

	BLASTER_API FVector GenerateRandomPatrolPointHorizontal(
		const FVector& Center,
		const FVector& Extent,
		float MinPatrolRadius,
		float MaxPatrolRadius);
}
