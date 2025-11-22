// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PatrolPointActor.generated.h"

/**
 * Patrol Point Actor - 순찰 포인트를 나타내는 액터
 * 레벨에 배치하여 드론의 순찰 경로를 정의
 */
UCLASS(BlueprintType, Blueprintable)
class BLASTER_API APatrolPointActor : public AActor
{
	GENERATED_BODY()
	
public:	
	APatrolPointActor();

	virtual void BeginPlay() override;

	/**
	 * 순찰 포인트에 도달했을 때 대기 시간
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol Point")
	float WaitTime = 2.0f;

	/**
	 * 이 포인트에서 드론이 회전할 각도 (옵션)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol Point")
	FRotator LookAtRotation = FRotator::ZeroRotator;

	/**
	 * 이 포인트에서 특정 방향을 바라볼지 여부
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol Point")
	bool bShouldLookAt = false;

protected:
	/**
	 * 시각적 표시를 위한 컴포넌트 (에디터에서 보기 쉽게)
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* VisualMesh;

	/**
	 * 디버그용 화살표 컴포넌트
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UArrowComponent* DirectionArrow;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};












