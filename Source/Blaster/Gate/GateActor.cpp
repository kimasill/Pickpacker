// Gate actor that unlocks when required item use actions are provided

#include "GateActor.h"
#include "Net/UnrealNetwork.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/Components/PlayerInventoryComponent.h"
#include "Blaster/Parcel/ParcelActor.h"
#include "Blaster/DataAssets/DA_ItemData.h"
#include "Blaster/Interaction/InteractionUIData.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"

AGateActor::AGateActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);
}

void AGateActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGateActor, bIsUnlocked);
}

bool AGateActor::CanInteract_Implementation(ACharacter* Interactor)
{
	if (bIsUnlocked || !Interactor)
	{
		return false;
	}

	// 서버 권위로 판별, 클라는 항상 허용하여 UI만 표시
	if (!HasAuthority())
	{
		return true;
	}

	return CheckGateConditions(Interactor);
}

void AGateActor::OnInteract_Implementation(ACharacter* Interactor)
{
	if (!HasAuthority() || !Interactor)
	{
		return;
	}

	if (bIsUnlocked)
	{
		return;
	}

	if (!CheckGateConditions(Interactor))
	{
		return;
	}

	UnlockGate(Interactor);
}

FText AGateActor::GetInteractText_Implementation()
{
	return FText::FromString(bIsUnlocked ? TEXT("Unlocked") : TEXT("Use Item"));
}

bool AGateActor::RequestShowInteractionUI_Implementation(ACharacter* Interactor)
{
	// 기본 위젯 사용 (false 반환 시 InteractionComponent가 기본 처리)
	return false;
}

void AGateActor::GetInteractionUIData_Implementation(FInteractionUIData& OutData)
{
	OutData.InteractionType = EInteractionType::Use;
	if (OutData.ActionText.IsEmpty())
	{
		OutData.ActionText = GetInteractText_Implementation();
	}

	// 필요한 UseAction 병합
	FGameplayTagContainer CombinedUseActions;
	for (const FGateCondition& Condition : RequiredConditions)
	{
		CombinedUseActions.AppendTags(Condition.RequiredUseActions);
	}
	OutData.RequiredUseActions = CombinedUseActions;

	if (UnlockResult.FailFeedbackText.IsEmpty() == false)
	{
		OutData.MissingRequirementsText = UnlockResult.FailFeedbackText;
	}
}

bool AGateActor::CheckGateConditions(ACharacter* User) const
{
	if (RequiredConditions.Num() == 0)
	{
		return true;
	}

	// 모든 플레이어가 인증해야 하는 경우
	if (bRequireAllPlayersAuthorized && HasAuthority())
	{
		if (AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr)
		{
			for (APlayerState* PS : GameState->PlayerArray)
			{
				if (!PS) continue;
				ACharacter* PlayerChar = Cast<ACharacter>(PS->GetPawn());
				if (!PlayerChar || !PlayerChar->IsPlayerControlled())
				{
					continue;
				}

				for (const FGateCondition& Condition : RequiredConditions)
				{
					if (!PlayerSatisfiesConditions(PlayerChar, Condition))
					{
						return false;
					}
				}
			}
			return true;
		}
	}

	// 단일 사용자 기준
	for (const FGateCondition& Condition : RequiredConditions)
	{
		if (!PlayerSatisfiesConditions(User, Condition))
		{
			return false;
		}
	}

	return true;
}

bool AGateActor::PlayerSatisfiesConditions(ACharacter* User, const FGateCondition& Condition) const
{
	if (!User)
	{
		return false;
	}

	// 플레이어 수 요구
	if (Condition.RequiredPlayerCount > 0 && HasAuthority())
	{
		int32 PlayerCount = 0;
		if (AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr)
		{
			PlayerCount = GameState->PlayerArray.Num();
		}
		if (PlayerCount < Condition.RequiredPlayerCount)
		{
			return false;
		}
	}

	// 월드 플래그 요구
	if (Condition.RequiredWorldFlag.IsValid())
	{
		const bool* FlagValue = WorldFlagState.Find(Condition.RequiredWorldFlag);
		if (!FlagValue || !(*FlagValue))
		{
			return false;
		}
	}

	// 인벤토리 요구 검사
	UPlayerInventoryComponent* Inventory = FindInventory(User);
	if (!Inventory)
	{
		return false;
	}

	const TArray<AParcelActor*> Items = Inventory->GetCollectedItems();

	// UseAction 검사 (OR)
	if (Condition.RequiredUseActions.Num() > 0)
	{
		bool bHasRequiredUseAction = false;
		for (AParcelActor* Item : Items)
		{
			if (!Item) continue;
			const FItemData& ItemData = Item->GetItemData();
			if (ItemData.UseActions.HasAnyExact(Condition.RequiredUseActions))
			{
				bHasRequiredUseAction = true;
				break;
			}
		}

		if (!bHasRequiredUseAction)
		{
			return false;
		}
	}

	// ItemId 검사
	if (Condition.RequiredItemId.IsValid())
	{
		bool bHasRequiredItemId = false;
		for (AParcelActor* Item : Items)
		{
			if (!Item) continue;
			const FItemData& ItemData = Item->GetItemData();
			if (ItemData.ItemId.IsValid() && ItemData.ItemId.MatchesTagExact(Condition.RequiredItemId))
			{
				bHasRequiredItemId = true;
				break;
			}
		}

		if (!bHasRequiredItemId)
		{
			return false;
		}
	}

	return true;
}

UPlayerInventoryComponent* AGateActor::FindInventory(const ACharacter* User) const
{
	const ABlasterCharacter* BlasterChar = Cast<ABlasterCharacter>(User);
	return BlasterChar ? BlasterChar->GetPlayerInventoryComponent() : nullptr;
}

bool AGateActor::DoesPlayerMeetConditions(ACharacter* User) const
{
	return CheckGateConditions(User);
}

bool AGateActor::TryUseItemWithGate(AParcelActor* Item, ACharacter* User)
{
	if (!HasAuthority() || !Item || !User || bIsUnlocked)
	{
		return false;
	}

	// 기본 조건(인벤토리/플래그) 충족 여부
	if (!CheckGateConditions(User))
	{
		return false;
	}

	// 아이템 자체가 조건과 맞는지 추가 확인 (UseAction / ItemId)
	const FItemData& ItemData = Item->GetItemData();
	for (const FGateCondition& Condition : RequiredConditions)
	{
		if (Condition.RequiredUseActions.Num() > 0 && !ItemData.UseActions.HasAnyExact(Condition.RequiredUseActions))
		{
			return false;
		}
		if (Condition.RequiredItemId.IsValid() && (!ItemData.ItemId.IsValid() || !ItemData.ItemId.MatchesTagExact(Condition.RequiredItemId)))
		{
			return false;
		}
	}

	UnlockGate(User);
	return true;
}

void AGateActor::UnlockGate(ACharacter* InstigatorCharacter)
{
	bIsUnlocked = true;
	OnGateUnlocked.Broadcast(this);

	// 결과 플래그 적용
	for (const TPair<FGameplayTag, bool>& Pair : UnlockResult.WorldFlagsToSet)
	{
		WorldFlagState.Add(Pair.Key, Pair.Value);
	}

	// 추가 효과(사운드/애니메이션)는 BP에서 처리하도록 UnlockAction 태그를 노출
	Multicast_OnGateUnlocked(InstigatorCharacter);
}

void AGateActor::OnRep_IsUnlocked()
{
	if (bIsUnlocked)
	{
		OnGateUnlocked.Broadcast(this);
	}
}

// 멀티캐스트: 언락 알림 (파생 BP에서 처리)
void AGateActor::Multicast_OnGateUnlocked_Implementation(ACharacter* InstigatorCharacter)
{
    // BP에서 구현 가능: 이펙트/사운드/애니메이션 등
    BP_OnGateUnlocked(InstigatorCharacter);
}
