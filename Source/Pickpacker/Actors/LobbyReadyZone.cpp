#include "LobbyReadyZone.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Blaster/GameMode/LobbyGameMode.h"

ALobbyReadyZone::ALobbyReadyZone()
{
	bReplicates = true;
	SetCanBeDamaged(false);
}

void ALobbyReadyZone::BeginPlay()
{
	Super::BeginPlay();

	OnActorBeginOverlap.AddDynamic(this, &ALobbyReadyZone::OnZoneBeginOverlap);
	OnActorEndOverlap.AddDynamic(this, &ALobbyReadyZone::OnZoneEndOverlap);
}

void ALobbyReadyZone::OnZoneBeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	UpdateReadyState(OtherActor, true);
}

void ALobbyReadyZone::OnZoneEndOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	UpdateReadyState(OtherActor, false);
}

void ALobbyReadyZone::UpdateReadyState(AActor* OtherActor, bool bReady)
{
	if (!HasAuthority() || OtherActor == nullptr)
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC)
	{
		return;
	}

	if (ALobbyGameMode* LobbyGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ALobbyGameMode>() : nullptr)
	{
		LobbyGameMode->SetPlayerReadyStatus(PC, bReady);
	}
}


