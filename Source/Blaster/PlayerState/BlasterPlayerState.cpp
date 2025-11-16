
#include "BlasterPlayerState.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Blaster/AI/MotherAIActor.h"
#include "Blaster/AI/DroneActor.h"
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

	// 의심 수치가 100에 도달하면 마더에게 알림
	if (PersonalSuspicion >= 100.0f && OldSuspicion < 100.0f)
	{
		// 마더 AI 찾기
		AMotherAIActor* MotherAI = nullptr;
		for (TActorIterator<AMotherAIActor> ActorItr(GetWorld()); ActorItr; ++ActorItr)
		{
			MotherAI = *ActorItr;
			break;
		}

		if (MotherAI)
		{
			ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(GetPawn());
			if (BlasterCharacter)
			{
				MotherAI->RequestPunishment(BlasterCharacter);
			}
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
