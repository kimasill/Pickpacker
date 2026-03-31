#include "ChuteEntryVolume.h"

#include "Components/BoxComponent.h"
#include "Blaster/GameState/PickpackerGameState.h"
#include "Blaster/Components/EscapeProgressComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"

AChuteEntryVolume::AChuteEntryVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	TriggerVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerVolume"));
	SetRootComponent(TriggerVolume);
	TriggerVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void AChuteEntryVolume::BeginPlay()
{
	Super::BeginPlay();

	if (TriggerVolume)
	{
		TriggerVolume->OnComponentBeginOverlap.AddDynamic(this, &AChuteEntryVolume::OnTriggerBegin);
		TriggerVolume->OnComponentEndOverlap.AddDynamic(this, &AChuteEntryVolume::OnTriggerEnd);
	}
}

void AChuteEntryVolume::OnTriggerBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
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

void AChuteEntryVolume::OnTriggerEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
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

void AChuteEntryVolume::EvaluateAllPlayersInside()
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
					// 진입 시 즉시 엔딩 재평가
					Progress->EvaluateEndings();
				}
			}
		}
	}
}

UEscapeProgressComponent* AChuteEntryVolume::GetEscapeProgress() const
{
	if (const APickpackerGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APickpackerGameState>() : nullptr)
	{
		return GameState->GetEscapeProgressComponent();
	}
	return nullptr;
}



























