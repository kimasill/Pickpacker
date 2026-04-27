// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AI/MotherAIActor.h"
#include "AI/PPAIAnimInstanceBase.h"
#include "MotherAnimInstance.generated.h"

class AMotherAIActor;

/**
 * 
 */
UCLASS()
class PICKPACKER_API UMotherAnimInstance : public UPPAIAnimInstanceBase
{
	GENERATED_BODY()

protected:
	virtual void CacheCharacterOwner() override;
	virtual void UpdateCharacterSpecificData(float DeltaTime) override;

private:
	/** Cached reference to Mother AI Actor */
	UPROPERTY(BlueprintReadOnly, Category = "Character", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AMotherAIActor> MotherAI = nullptr;

	/** AI State */
	UPROPERTY(BlueprintReadOnly, Category = "AI State", meta = (AllowPrivateAccess = "true"))
	EMotherAIState CurrentAIState = EMotherAIState::Normal;

	/** 처벌 몽타주 재생 중 여부 (스레드 안전) */
	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	bool bIsPlayingPunishmentMontage = false;

	/** 점검 몽타주 재생 중 여부 (스레드 안전) */
	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	bool bIsPlayingInspectionMontage = false;
};
