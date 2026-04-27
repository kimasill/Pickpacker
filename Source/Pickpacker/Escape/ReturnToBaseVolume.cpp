#include "Escape/ReturnToBaseVolume.h"

#include "Components/BoxComponent.h"
#include "Components/EscapeProgressComponent.h"
#include "Components/TrainTravelComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameState/PickpackerGameState.h"
#include "Subsystem/CoreLoopSubsystem.h"
#include "Subsystem/RunPersistenceSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

AReturnToBaseVolume::AReturnToBaseVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	TriggerVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerVolume"));
	SetRootComponent(TriggerVolume);
	TriggerVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void AReturnToBaseVolume::BeginPlay()
{
	Super::BeginPlay();

	if (TriggerVolume)
	{
		TriggerVolume->OnComponentBeginOverlap.AddDynamic(this, &AReturnToBaseVolume::OnTriggerBegin);
		TriggerVolume->OnComponentEndOverlap.AddDynamic(this, &AReturnToBaseVolume::OnTriggerEnd);
	}
}

void AReturnToBaseVolume::OnTriggerBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || bReturnTriggered)
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character || !Character->IsPlayerControlled())
	{
		return;
	}

	if (AController* Controller = Character->GetController())
	{
		ControllersInside.Add(Controller);
		EvaluateReturnCondition();
	}
}

void AReturnToBaseVolume::OnTriggerEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!HasAuthority() || bReturnTriggered)
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (!Character || !Character->IsPlayerControlled())
	{
		return;
	}

	if (AController* Controller = Character->GetController())
	{
		ControllersInside.Remove(Controller);
	}
}

void AReturnToBaseVolume::EvaluateReturnCondition()
{
	if (!HasAuthority() || bReturnTriggered)
	{
		return;
	}

	if (!bRequireAllPlayersInside)
	{
		TriggerReturnToBase();
		return;
	}

	if (const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr)
	{
		const int32 TotalPlayers = GameState->PlayerArray.Num();
		if (TotalPlayers > 0 && ControllersInside.Num() >= TotalPlayers)
		{
			TriggerReturnToBase();
		}
	}
}

void AReturnToBaseVolume::TriggerReturnToBase()
{
	if (!HasAuthority() || bReturnTriggered)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APickpackerGameState* PickpackerGameState = World->GetGameState<APickpackerGameState>();
	UCoreLoopSubsystem* CoreLoop = World->GetSubsystem<UCoreLoopSubsystem>();
	if (!PickpackerGameState || !CoreLoop || !CoreLoop->IsRunActive())
	{
		return;
	}

	bReturnTriggered = true;
	CoreLoop->ReturnToBase();

	UTrainTravelComponent* TrainTravel = PickpackerGameState->GetTrainTravelComponent();
	const TArray<FStorageRecord> CargoRecords = TrainTravel ? TrainTravel->GetCargoRecords() : TArray<FStorageRecord>();

	if (bResetTrainState)
	{
		if (TrainTravel)
		{
			TrainTravel->ResetTrain();
		}
	}

	if (UGameInstance* GameInstance = World->GetGameInstance())
	{
		if (URunPersistenceSubsystem* RunPersistence = GameInstance->GetSubsystem<URunPersistenceSubsystem>())
		{
			const TArray<FWorldFlagEntry>& WorldFlags = PickpackerGameState->GetEscapeProgressComponent()
				? PickpackerGameState->GetEscapeProgressComponent()->GetWorldFlags()
				: TArray<FWorldFlagEntry>();

			RunPersistence->SaveSnapshot(
				CoreLoop->GetRunState(),
				CoreLoop->GetCurrentDestination(),
				PickpackerGameState->GetCurrentRouteSelectionResult(),
				WorldFlags,
				CargoRecords);
		}
	}

	if (!bTravelToBaseLevel || BaseLevel.IsNull())
	{
		return;
	}

	const FString BaseLevelPath = BaseLevel.GetLongPackageName();
	if (!BaseLevelPath.IsEmpty())
	{
		World->ServerTravel(BaseLevelPath + TEXT("?listen"), true);
	}
}
