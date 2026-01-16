#include "MenuGameMode.h"

#include "Blaster/PlayerController/MenuPlayerController.h"
#include "Camera/CameraActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

AMenuGameMode::AMenuGameMode()
{
	// 메뉴에서는 Pawn이 필요 없으므로 스폰을 막고, 전용 PlayerController를 사용
	DefaultPawnClass = nullptr;
	PlayerControllerClass = AMenuPlayerController::StaticClass();
	bStartPlayersAsSpectators = true;
}

void AMenuGameMode::BeginPlay()
{
	Super::BeginPlay();

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ApplyMenuSetupToController(It->Get());
	}
}

void AMenuGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	ApplyMenuSetupToController(NewPlayer);
}

void AMenuGameMode::ApplyMenuSetupToController(APlayerController* PlayerController)
{
	if (!PlayerController) return;

	if (AActor* CameraActor = ResolveInitialCamera())
	{
		// 즉시 전환: 메인 메뉴 입장 시 고정 시점 유지
		PlayerController->SetViewTargetWithBlend(CameraActor, 0.f);
	}

	if (AMenuPlayerController* MenuPC = Cast<AMenuPlayerController>(PlayerController))
	{
		MenuPC->ApplyMenuInputSettings();
	}
}

AActor* AMenuGameMode::ResolveInitialCamera() const
{
	if (InitialCameraActor)
	{
		return InitialCameraActor;
	}

	if (!MenuCameraTag.IsNone())
	{
		TArray<AActor*> TaggedActors;
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), MenuCameraTag, TaggedActors);
		for (AActor* Actor : TaggedActors)
		{
			if (Actor && Actor->IsA(ACameraActor::StaticClass()))
			{
				return Actor;
			}
		}
		if (TaggedActors.Num() > 0)
		{
			return TaggedActors[0];
		}
	}

	if (bFallbackToAnyCamera)
	{
		TArray<AActor*> Cameras;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACameraActor::StaticClass(), Cameras);
		if (Cameras.Num() > 0)
		{
			return Cameras[0];
		}
	}

	return nullptr;
}

