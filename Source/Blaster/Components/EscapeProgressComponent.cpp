#include "EscapeProgressComponent.h"

#include "Net/UnrealNetwork.h"
#include "Blaster/DataAssets/DA_EndingData.h"
#include "Blaster/GameState/PickpackerGameState.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"

UEscapeProgressComponent::UEscapeProgressComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

FWorldFlagEntry* UEscapeProgressComponent::FindWorldFlagEntry(const FGameplayTag& Flag)
{
	return WorldFlags.FindByPredicate([&Flag](const FWorldFlagEntry& Entry)
		{
			return Entry.Flag == Flag;
		});
}

const FWorldFlagEntry* UEscapeProgressComponent::FindWorldFlagEntry(const FGameplayTag& Flag) const
{
	return WorldFlags.FindByPredicate([&Flag](const FWorldFlagEntry& Entry)
		{
			return Entry.Flag == Flag;
		});
}

void UEscapeProgressComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UEscapeProgressComponent, WorldFlags);
	DOREPLIFETIME(UEscapeProgressComponent, RouteProgress);
	DOREPLIFETIME(UEscapeProgressComponent, AuthorizedPlayerCount);
	DOREPLIFETIME(UEscapeProgressComponent, CurrentEndingId);
}

void UEscapeProgressComponent::SetWorldFlag(const FGameplayTag& Flag, int32 Value)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (FWorldFlagEntry* Entry = FindWorldFlagEntry(Flag))
	{
		Entry->Value = Value;
	}
	else
	{
		FWorldFlagEntry NewEntry;
		NewEntry.Flag = Flag;
		NewEntry.Value = Value;
		WorldFlags.Add(MoveTemp(NewEntry));
	}

	// 월드 플래그 변경 시 즉시 엔딩 조건 재평가
	EvaluateEndings();
}

int32 UEscapeProgressComponent::GetWorldFlag(const FGameplayTag& Flag) const
{
	if (const FWorldFlagEntry* Entry = FindWorldFlagEntry(Flag))
	{
		return Entry->Value;
	}
	return 0;
}

void UEscapeProgressComponent::AddAuthorizedPlayer(APlayerState* PlayerState)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !PlayerState)
	{
		return;
	}

	const TWeakObjectPtr<APlayerState> Key(PlayerState);
	if (!AuthorizedPlayers.Contains(Key))
	{
		AuthorizedPlayers.Add(Key);
		AuthorizedPlayerCount = AuthorizedPlayers.Num();
		EvaluateEndings();
	}
}

bool UEscapeProgressComponent::StartEndingById(const FName& EndingId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || EndingId.IsNone())
	{
		return false;
	}

	for (const UDA_EndingData* EndingData : EndingDataAssets)
	{
		if (EndingData && EndingData->EndingId == EndingId)
		{
			StartEnding(EndingData);
			return true;
		}
	}
	return false;
}

void UEscapeProgressComponent::EvaluateEndings()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	// 이미 엔딩이 시작된 경우 추가 트리거 방지
	if (!CurrentEndingId.IsNone())
	{
		return;
	}

	for (const UDA_EndingData* EndingData : EndingDataAssets)
	{
		if (!EndingData)
		{
			continue;
		}

		if (IsEndingConditionMet(EndingData))
		{
			StartEnding(EndingData);
			return;
		}
	}
}

bool UEscapeProgressComponent::IsEndingConditionMet(const UDA_EndingData* EndingData) const
{
	if (!EndingData)
	{
		return false;
	}

	for (const FGameplayTag& Flag : EndingData->RequiredWorldFlags)
	{
		const int32 Value = GetWorldFlag(Flag);
		if (Value <= 0)
		{
			return false;
		}
	}

	if (EndingData->RequiredAuthorizedPlayers > 0 && AuthorizedPlayerCount < EndingData->RequiredAuthorizedPlayers)
	{
		return false;
	}

	return true;
}

void UEscapeProgressComponent::StartEnding(const UDA_EndingData* EndingData)
{
	if (!EndingData || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (!CurrentEndingId.IsNone())
	{
		return;
	}

	CurrentEndingId = EndingData->EndingId;

	// 모든 플레이어 입력 차단 및 HUD 전환 트리거
	BroadcastInputBlock(true);

	// 엔딩 시퀀스 재생
	Multicast_PlayEndingSequence(EndingData->EndingSequence);
}

void UEscapeProgressComponent::BroadcastInputBlock(bool bBlocked)
{
	if (!GetWorld())
	{
		return;
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABlasterPlayerController* PC = Cast<ABlasterPlayerController>(It->Get()))
		{
			PC->SetInputBlocked(bBlocked);
		}
	}
}

void UEscapeProgressComponent::Multicast_PlayEndingSequence_Implementation(ULevelSequence* Sequence)
{
	if (!Sequence || !GetWorld())
	{
		return;
	}

	ALevelSequenceActor* OutActor = nullptr;
	FMovieSceneSequencePlaybackSettings Settings;
	Settings.bPauseAtEnd = true;

	if (ULevelSequencePlayer* Player = ULevelSequencePlayer::CreateLevelSequencePlayer(GetWorld(), Sequence, Settings, OutActor))
	{
		Player->Play();
	}
}

void UEscapeProgressComponent::OnRep_CurrentEndingId()
{
	// 클라이언트에서 엔딩 시작 시 입력 차단
	if (!CurrentEndingId.IsNone())
	{
		BroadcastInputBlock(true);
	}
}


