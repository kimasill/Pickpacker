#pragma once

#include "CoreMinimal.h"
#include "AI/BruteActor.h"
#include "AI/PPAIAnimInstanceBase.h"
#include "BruteAnimInstance.generated.h"

class ABruteActor;

/**
 * Thin Brute-specific layer on top of the shared AI locomotion anim base.
 */
UCLASS()
class PICKPACKER_API UBruteAnimInstance : public UPPAIAnimInstanceBase
{
	GENERATED_BODY()

protected:
	virtual void CacheCharacterOwner() override;
	virtual void UpdateCharacterSpecificData(float DeltaTime) override;

private:
	UPROPERTY(BlueprintReadOnly, Category = "Character", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ABruteActor> BruteAI = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "AI State", meta = (AllowPrivateAccess = "true"))
	EBruteState CurrentBruteState = EBruteState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	bool bIsPlayingExecutionMontage = false;
};
