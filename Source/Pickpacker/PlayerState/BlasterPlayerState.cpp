
#include "BlasterPlayerState.h"
#include "Character/BlasterCharacter.h"
#include "PlayerController/BlasterPlayerController.h"
#include "AI/MotherAIActor.h"
#include "AI/DroneActor.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "EngineUtils.h"

void ABlasterPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABlasterPlayerState, Defeats);
	DOREPLIFETIME(ABlasterPlayerState, Team);
	DOREPLIFETIME(ABlasterPlayerState, PersonalSuspicion);
	DOREPLIFETIME(ABlasterPlayerState, Lives);
	DOREPLIFETIME(ABlasterPlayerState, bIsReady);
}

void ABlasterPlayerState::AddToScore(float ScoreAmount)
{
	SetScore(GetScore() + ScoreAmount);
	Character = Character == nullptr ? Cast<ABlasterCharacter>(GetPawn()) : Character;
	if (Character)
	{
		Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
		if (Controller)
		{
			Controller->SetHUDScore(GetScore());
		}
	}
}

void ABlasterPlayerState::AddToDefeats(int32 DefeatAmount)
{
	Defeats += + DefeatAmount;
	Character = Character == nullptr ? Cast<ABlasterCharacter>(GetPawn()) : Character;
	if (Character)
	{
		Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
		if (Controller)
		{
			Controller->SetHUDDefeats(Defeats);
		}
	}
}
void ABlasterPlayerState::OnRep_Score()
{
	Super::OnRep_Score();

	GetPawn();

	Character = Character == nullptr ? Cast<ABlasterCharacter>(GetPawn()) : Character;
	if (Character)
	{
		Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
		if (Controller)
		{
			Controller->SetHUDScore(GetScore());
		}
	}
}

void ABlasterPlayerState::OnRep_Defeats()
{
	Character = Character == nullptr ? Cast<ABlasterCharacter>(GetPawn()) : Character;
	if (Character)
	{
		Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
		if (Controller)
		{
			Controller->SetHUDDefeats(Defeats);
		}
	}
}

void ABlasterPlayerState::SetTeam(ETeam TeamToSet)
{
	Team = TeamToSet;
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(GetPawn());
	if(BlasterCharacter)
	{
		BlasterCharacter->SetTeamColor(Team);
	}
}


void ABlasterPlayerState::OnRep_Team()
{
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(GetPawn());
	if (BlasterCharacter)
	{
		BlasterCharacter->SetTeamColor(Team);
	}
}

void ABlasterPlayerState::AddPersonalSuspicion(float Points)
{
	if (!HasAuthority())
	{
		return;
	}

	float OldSuspicion = PersonalSuspicion;
	PersonalSuspicion = FMath::Clamp(PersonalSuspicion + Points, 0.0f, 100.0f);

	UE_LOG(LogTemp, Log, TEXT("[BlasterPlayerState] Player %s suspicion: %.2f -> %.2f (+%.2f)"), 
		*GetPlayerName(), OldSuspicion, PersonalSuspicion, Points);

	OnPersonalSuspicionChanged.Broadcast(PersonalSuspicion, OldSuspicion);

	// 의심 수치가 100에 도달하면 이벤트만 브로드캐스트 (마더 AI는 이벤트를 구독하여 처리)
	// PlayerState는 MotherAI를 직접 참조하지 않아 의존성이 제거됨
	if (PersonalSuspicion >= 100.0f && OldSuspicion < 100.0f)
	{
		ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(GetPawn());
		if (BlasterCharacter)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BlasterPlayerState] Player %s suspicion reached 100, event will be handled by subscribers"), 
				*GetPlayerName());
		}
	}
}

void ABlasterPlayerState::LoseLife()
{
	if (!HasAuthority())
	{
		return;
	}

	int32 OldLives = Lives;
	Lives = FMath::Max(0, Lives - 1);

	UE_LOG(LogTemp, Warning, TEXT("[BlasterPlayerState] Player %s lost a life: %d -> %d"), 
		*GetPlayerName(), OldLives, Lives);

	OnLivesChanged.Broadcast(Lives, OldLives);

	// 목숨이 모두 소진되면 드론 추격 시작
	if (Lives <= 0 && OldLives > 0)
	{
		ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(GetPawn());
		if (BlasterCharacter)
		{
			BlasterCharacter->HandleOutOfLives();

			// 모든 드론에게 추격 명령
			for (TActorIterator<ADroneActor> ActorItr(GetWorld()); ActorItr; ++ActorItr)
			{
				ADroneActor* Drone = *ActorItr;
				if (Drone && Drone->IsActive())
				{
					Drone->StartChasing(BlasterCharacter);
				}
			}
		}
	}
}

void ABlasterPlayerState::OnRep_PersonalSuspicion(float OldSuspicion)
{
	OnPersonalSuspicionChanged.Broadcast(PersonalSuspicion, OldSuspicion);
}

void ABlasterPlayerState::OnRep_Lives(int32 OldLives)
{
	OnLivesChanged.Broadcast(Lives, OldLives);
}

void ABlasterPlayerState::SetReadyStatus(bool bReady)
{
	if (!HasAuthority())
	{
		return;
	}

	bool bOldReady = bIsReady;
	bIsReady = bReady;

	UE_LOG(LogTemp, Log, TEXT("[BlasterPlayerState] Player %s ready status: %s -> %s"),
		*GetPlayerName(), bOldReady ? TEXT("Ready") : TEXT("Not Ready"), bReady ? TEXT("Ready") : TEXT("Not Ready"));

	OnReadyStatusChanged.Broadcast(bIsReady, bOldReady);
}

void ABlasterPlayerState::OnRep_IsReady()
{
	// 자신의 캐릭터 위젯 업데이트
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(GetPawn());
	if (BlasterCharacter)
	{
		BlasterCharacter->UpdateOverheadWidget();
	}

	// 모든 클라이언트에서 모든 플레이어의 위젯 업데이트
	UWorld* World = GetWorld();
	if (World)
	{
		for (TActorIterator<ABlasterCharacter> It(World); It; ++It)
		{
			if (ABlasterCharacter* LocalCharacter = *It)
			{
				LocalCharacter->UpdateOverheadWidget();
			}
		}
	}
}
