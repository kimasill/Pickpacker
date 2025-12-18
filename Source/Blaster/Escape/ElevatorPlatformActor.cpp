#include "ElevatorPlatformActor.h"

#include "Components/BoxComponent.h"
#include "Blaster/GameState/PickpackerGameState.h"
#include "Blaster/Components/EscapeProgressComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"

AElevatorPlatformActor::AElevatorPlatformActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	TriggerVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerVolume"));
	SetRootComponent(TriggerVolume);
	TriggerVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void AElevatorPlatformActor::BeginPlay()
{
	Super::BeginPlay();

	if (TriggerVolume)
	{
		TriggerVolume->OnComponentBeginOverlap.AddDynamic(this, &AElevatorPlatformActor::OnTriggerBegin);
		TriggerVolume->OnComponentEndOverlap.AddDynamic(this, &AElevatorPlatformActor::OnTriggerEnd);
	}
}

void AElevatorPlatformActor::OnTriggerBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority())
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
		EvaluateAllPlayersInside();
	}
}

void AElevatorPlatformActor::OnTriggerEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!HasAuthority())
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

void AElevatorPlatformActor::EvaluateAllPlayersInside()
{
	if (!HasAuthority())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (const AGameStateBase* GameState = World->GetGameState())
		{
			const int32 TotalPlayers = GameState->PlayerArray.Num();
			if (TotalPlayers > 0 && ControllersInside.Num() >= TotalPlayers)
			{
				if (UEscapeProgressComponent* Progress = GetEscapeProgress())
				{
					if (AllPlayersInsideFlag.IsValid())
					{
						Progress->SetWorldFlag(AllPlayersInsideFlag, 1);
					}
					Progress->EvaluateEndings();
				}
			}
		}
	}
}

UEscapeProgressComponent* AElevatorPlatformActor::GetEscapeProgress() const
{
	if (const APickpackerGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APickpackerGameState>() : nullptr)
	{
		return GameState->GetEscapeProgressComponent();
	}
	return nullptr;
}

