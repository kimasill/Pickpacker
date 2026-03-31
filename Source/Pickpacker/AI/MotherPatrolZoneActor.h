#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MotherPatrolZoneActor.generated.h"

class UBoxComponent;

/**
 * Mother AI 순찰 구역 액터.
 * 레벨에 배치하여 점검(순찰) 구역의 범위를 지정합니다.
 * - Box 볼륨으로 순찰 범위 설정
 * - MotherInspectionPoint 태그로 자동 검색 (BP_Mother InspectionActorTag 폴백)
 * - BTTask_MotherPatrolInVolume, BTTask_MotherGeneratePatrolPoint 등과 연동
 */
UCLASS(Blueprintable)
class BLASTER_API AMotherPatrolZoneActor : public AActor
{
	GENERATED_BODY()

public:
	AMotherPatrolZoneActor();

	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** 볼륨 중앙 위치 반환 (순찰/점검 목표 위치) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Mother Patrol Zone")
	FVector GetZoneCenter() const;

	/** 볼륨 범위(Half-Extent) 반환 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Mother Patrol Zone")
	FVector GetZoneExtent() const;

	/** 순찰 구역 크기 (Half-Extent, XYZ 각 축의 반지름) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother Patrol Zone")
	FVector BoxExtent = FVector(200.0f, 200.0f, 100.0f);

	/** 구역 식별용 이름 (디버깅/로그) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mother Patrol Zone")
	FString ZoneName;

protected:
	/** 순찰 범위 볼륨 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> PatrolVolume;
};
