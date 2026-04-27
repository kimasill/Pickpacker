#include "BruteAnimInstance.h"

#include "AI/BruteActor.h"
#include "Components/SkeletalMeshComponent.h"

void UBruteAnimInstance::CacheCharacterOwner()
{
	Super::CacheCharacterOwner();
	BruteAI = Cast<ABruteActor>(GetCharacterOwner());
}

void UBruteAnimInstance::UpdateCharacterSpecificData(float /*DeltaTime*/)
{
	if (BruteAI == nullptr)
	{
		CacheCharacterOwner();
	}

	if (BruteAI == nullptr)
	{
		return;
	}

	CurrentBruteState = BruteAI->GetState();

	if (UAnimInstance* AnimInstance = BruteAI->GetMesh() ? BruteAI->GetMesh()->GetAnimInstance() : nullptr)
	{
		bIsPlayingExecutionMontage = BruteAI->ExecutionMontage != nullptr &&
			AnimInstance->Montage_IsPlaying(BruteAI->ExecutionMontage);
	}
	else
	{
		bIsPlayingExecutionMontage = false;
	}
}
