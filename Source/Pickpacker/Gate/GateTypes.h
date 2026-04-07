// Gameplay gate-related structs for item-based interactions
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GateTypes.generated.h"

/**
 * 게이트 해제에 필요한 조건 세트
 * 모든 조건은 AND로 평가되며, 각 필드 내부는 주석에 따라 OR/단일 매칭.
 */
USTRUCT(BlueprintType)
struct PICKPACKER_API FGateCondition
{
	GENERATED_BODY()

	/** 요구되는 UseAction 태그 (이 컨테이너 내부는 OR) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gate")
	FGameplayTagContainer RequiredUseActions;

	/** 특정 아이템 ID 요구 (Optional) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gate")
	FGameplayTag RequiredItemId;

	/** 특정 월드 상태 플래그 요구 (Optional) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gate")
	FGameplayTag RequiredWorldFlag;

	/** 필요한 플레이어 수 (예: 인증 다중 요구) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gate")
	int32 RequiredPlayerCount = 0;
};

/**
 * 게이트 해제 시 수행할 결과 정보
 */
USTRUCT(BlueprintType)
struct PICKPACKER_API FGateUnlockResult
{
	GENERATED_BODY()

	/** 결과 액션 태그 (예: DoorOpen, PanelDestroyed 등) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gate")
	FGameplayTag UnlockAction;

	/** 해제 시 설정할 월드 플래그 값 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gate")
	TMap<FGameplayTag, bool> WorldFlagsToSet;

	/** 실패 시 피드백 텍스트 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gate")
	FText FailFeedbackText;
};
