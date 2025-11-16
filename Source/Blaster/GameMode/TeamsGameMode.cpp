// Fill out your copyright notice in the Description page of Project Settings.


#include "TeamsGameMode.h"
#include "Blaster/GameState/BlasterGameState.h"
#include "Blaster/PlayerState/BlasterPlayerState.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Kismet/GameplayStatics.h"

ATeamsGameMode::ATeamsGameMode()
{
	bTeamsMatch = true;
}

void ATeamsGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	ABlasterGameState* BGameState = Cast<ABlasterGameState>(UGameplayStatics::GetGameState(this));
	if (BGameState)
	{
		ABlasterPlayerState* BPlayerState = NewPlayer->GetPlayerState<ABlasterPlayerState>();
		if (BPlayerState && BPlayerState->GetTeam() == ETeam::ET_NoTeam)
		{
			if (BGameState->BlueTeam.Num() <= BGameState->RedTeam.Num())
			{
				BPlayerState->SetTeam(ETeam::ET_BlueTeam);
				BGameState->BlueTeam.Add(BPlayerState);
			}
			else
			{
				BPlayerState->SetTeam(ETeam::ET_RedTeam);
				BGameState->RedTeam.Add(BPlayerState);
			}

		}
	}
}

void ATeamsGameMode::Logout(AController* Exiting)
{
	ABlasterGameState* BGameState = Cast<ABlasterGameState>(UGameplayStatics::GetGameState(this));
	ABlasterPlayerState* BPlayerState = Exiting->GetPlayerState<ABlasterPlayerState>();
	if(BGameState && BPlayerState)
	{
		if (BGameState->BlueTeam.Contains(BPlayerState))
		{
			BGameState->BlueTeam.Remove(BPlayerState);
		}
		else if (BGameState->RedTeam.Contains(BPlayerState))
		{
			BGameState->RedTeam.Remove(BPlayerState);
		}
	}
}

void ATeamsGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();

	ABlasterGameState* BGameState = Cast<ABlasterGameState>(UGameplayStatics::GetGameState(this));
	if (BGameState)
	{
		for (auto PState : BGameState->PlayerArray)
		{
			ABlasterPlayerState* BPlayerState = Cast<ABlasterPlayerState>(PState);
			if (BPlayerState && BPlayerState->GetTeam() == ETeam::ET_NoTeam)
			{
				if (BGameState->BlueTeam.Num() <= BGameState->RedTeam.Num())
				{
					BPlayerState->SetTeam(ETeam::ET_BlueTeam);
					BGameState->BlueTeam.Add(BPlayerState);
				}
				else
				{
					BPlayerState->SetTeam(ETeam::ET_RedTeam);
					BGameState->RedTeam.Add(BPlayerState);
				}
			}
		}		
	}		
}
float ATeamsGameMode::CalculateDamage(AController* Attacker, AController* Victim, float BaseDamage)
{
	ABlasterPlayerState* AttackerPlayerState = Attacker->GetPlayerState<ABlasterPlayerState>();
	ABlasterPlayerState* VictimPlayerState = Victim->GetPlayerState<ABlasterPlayerState>();
	if(AttackerPlayerState == nullptr || VictimPlayerState == nullptr) return BaseDamage;
	if (VictimPlayerState == AttackerPlayerState) return BaseDamage; 
	if(AttackerPlayerState->GetTeam() == VictimPlayerState->GetTeam())
	{
		return 0.f; // No friendly fire
	}
	return BaseDamage;
}

void ATeamsGameMode::PlayerEliminated(class ABlasterCharacter* EliminatedCharacter, class ABlasterPlayerController* VictimController, class ABlasterPlayerController* AttackerController)
{
	Super::PlayerEliminated(EliminatedCharacter, VictimController, AttackerController);

	ABlasterGameState* BGameState = Cast<ABlasterGameState>(UGameplayStatics::GetGameState(this));
	ABlasterPlayerState* AttackerPlayerState = AttackerController ? Cast<ABlasterPlayerState>(AttackerController->PlayerState) : nullptr;
	if (BGameState && AttackerPlayerState)
	{
		if(AttackerPlayerState->GetTeam() == ETeam::ET_BlueTeam)
		{
			BGameState->BlueTeamScores();
		}
		else if (AttackerPlayerState->GetTeam() == ETeam::ET_RedTeam)
		{
			BGameState->RedTeamScores();
		}
	}
}