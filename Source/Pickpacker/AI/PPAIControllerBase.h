// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "PPAIControllerBase.generated.h"

class UBehaviorTree;
class UBlackboardData;

/**
 * Pickpacker 공통 AI 컨트롤러 베이스 — CrowdFollowing + 동일한 BT/BB 초기화 경로.
 */
UCLASS(Abstract)
class PICKPACKER_API APPAIControllerBase : public AAIController
{
	GENERATED_BODY()

public:
	APPAIControllerBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void OnUnPossess() override;

	/**
	 * Blackboard 초기화 후 Behavior Tree 실행.
	 * @param BTAsset 실행할 BT (BlackboardAsset이 있어야 함)
	 * @param BBOverride BT와 별도로 지정할 BB (null이면 BT의 BlackboardAsset 사용)
	 */
	UFUNCTION(BlueprintCallable, Category = "PP|AI")
	bool RunBehaviorTreeWithBlackboard(UBehaviorTree* BTAsset, UBlackboardData* BBOverride = nullptr);

	UFUNCTION(BlueprintCallable, Category = "PP|AI")
	void StopBehaviorTreeIfRunning();

protected:
	bool bPPBehaviorTreeRunning = false;
};
