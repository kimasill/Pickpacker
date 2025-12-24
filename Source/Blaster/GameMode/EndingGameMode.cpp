#include "EndingGameMode.h"

#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "GameFramework/PlayerStart.h"

AEndingGameMode::AEndingGameMode()
{
	// 엔딩 맵은 짧은 연출용이므로 틱 불필요
	PrimaryActorTick.bCanEverTick = false;
}

void AEndingGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (ABlasterPlayerController* PC = Cast<ABlasterPlayerController>(NewPlayer))
	{
		ApplyEndingSetup(PC);
	}
}

void AEndingGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	if (ABlasterPlayerController* PC = Cast<ABlasterPlayerController>(NewPlayer))
	{
		ApplyEndingSetup(PC);
	}
}

UClass* AEndingGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	if (EndingPawnClass)
	{
		return EndingPawnClass;
	}
	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

void AEndingGameMode::OnEndingPlayerReady(ABlasterPlayerController* PlayerController)
{
	if (EndingSequenceAsset.IsValid())
	{
		PlayerController->ClientPlayEndingSequence(EndingSequenceAsset);
	}
}
void AEndingGameMode::ApplyEndingSetup(ABlasterPlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return;
	}

	if (bBlockInputOnLogin)
	{
		PlayerController->SetInputBlocked(true);
	}

	// 블루프린트로 추가 연출(시퀀스/카메라/UI)을 트리거할 수 있도록 이벤트 호출
	OnEndingPlayerReady(PlayerController);
}

