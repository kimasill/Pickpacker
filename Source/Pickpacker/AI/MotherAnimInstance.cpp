#include "MotherAnimInstance.h"

#include "AI/MotherAIActor.h"
#include "Components/SkeletalMeshComponent.h"

void UMotherAnimInstance::CacheCharacterOwner()
{
	Super::CacheCharacterOwner();
	MotherAI = Cast<AMotherAIActor>(GetCharacterOwner());
}

void UMotherAnimInstance::UpdateCharacterSpecificData(float /*DeltaTime*/)
{
	if (MotherAI == nullptr)
	{
		CacheCharacterOwner();
	}

	if (MotherAI == nullptr)
	{
		return;
	}

	CurrentAIState = MotherAI->GetAIState();

	if (UAnimInstance* AnimInstance = MotherAI->GetMesh() ? MotherAI->GetMesh()->GetAnimInstance() : nullptr)
	{
		if (UAnimMontage* PunishmentMontage = MotherAI->GetPunishmentMontage())
		{
			bIsPlayingPunishmentMontage = AnimInstance->Montage_IsPlaying(PunishmentMontage);
		}
		else
		{
			bIsPlayingPunishmentMontage = false;
		}

		if (UAnimMontage* InspectionMontage = MotherAI->GetInspectionMontage())
		{
			bIsPlayingInspectionMontage = AnimInstance->Montage_IsPlaying(InspectionMontage);
		}
		else
		{
			bIsPlayingInspectionMontage = false;
		}
	}
	else
	{
		bIsPlayingPunishmentMontage = false;
		bIsPlayingInspectionMontage = false;
	}
}
