#include "Escape/ReturnTrainPlatformActor.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TrainTravelComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameState/PickpackerGameState.h"
#include "Subsystem/CoreLoopSubsystem.h"
#include "Engine/World.h"

AReturnTrainPlatformActor::AReturnTrainPlatformActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	BoardingVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("BoardingVolume"));
	BoardingVolume->SetupAttachment(SceneRoot);
	BoardingVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoardingVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoardingVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	BoardingVolume->SetBoxExtent(FVector(200.0f, 200.0f, 150.0f));

	InteractionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionVolume"));
	InteractionVolume->SetupAttachment(SceneRoot);
	InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionVolume->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Block);
	InteractionVolume->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractionVolume->SetBoxExtent(FVector(60.0f, 60.0f, 120.0f));
}

void AReturnTrainPlatformActor::BeginPlay()
{
	Super::BeginPlay();

	if (BoardingVolume)
	{
		BoardingVolume->OnComponentBeginOverlap.AddDynamic(this, &AReturnTrainPlatformActor::OnBoardingVolumeBeginOverlap);
		BoardingVolume->OnComponentEndOverlap.AddDynamic(this, &AReturnTrainPlatformActor::OnBoardingVolumeEndOverlap);
	}
}

void AReturnTrainPlatformActor::OnInteract_Implementation(ACharacter* Interactor)
{
	if (!HasAuthority() || !CanOperateDeparture(Interactor))
	{
		return;
	}

	if (UTrainTravelComponent* TrainTravel = GetTrainTravelComponent())
	{
		if (TrainTravel->GetTrainState() == ETrainState::Idle)
		{
			TrainTravel->BeginReturnBoarding();
		}

		if (TrainTravel->GetJourneyType() == ETrainJourneyType::ReturnToBase &&
			TrainTravel->GetTrainState() == ETrainState::Boarding)
		{
			TrainTravel->Depart();
		}
	}
}

bool AReturnTrainPlatformActor::CanInteract_Implementation(ACharacter* Interactor)
{
	if (!Interactor || !IsCharacterBoarded(Interactor))
	{
		return false;
	}

	if (const UTrainTravelComponent* TrainTravel = GetTrainTravelComponent())
	{
		if (TrainTravel->GetTrainState() == ETrainState::Idle)
		{
			if (const UWorld* World = GetWorld())
			{
				if (const UCoreLoopSubsystem* CoreLoop = World->GetSubsystem<UCoreLoopSubsystem>())
				{
					return CoreLoop->IsRunActive() && CoreLoop->GetCurrentPhase() == ECoreLoopPhase::Underground;
				}
			}
		}

		return TrainTravel->GetJourneyType() == ETrainJourneyType::ReturnToBase &&
			TrainTravel->GetTrainState() == ETrainState::Boarding;
	}

	return false;
}

FText AReturnTrainPlatformActor::GetInteractText_Implementation()
{
	if (const UTrainTravelComponent* TrainTravel = GetTrainTravelComponent())
	{
		if (TrainTravel->GetJourneyType() == ETrainJourneyType::ReturnToBase &&
			TrainTravel->GetTrainState() == ETrainState::Boarding)
		{
			return FText::FromString(TEXT("Operate Return Train"));
		}
	}

	return FText::FromString(TEXT("Board And Operate Return Train"));
}

bool AReturnTrainPlatformActor::RequestShowInteractionUI_Implementation(ACharacter* Interactor)
{
	return false;
}

void AReturnTrainPlatformActor::GetInteractionUIData_Implementation(FInteractionUIData& OutData)
{
	OutData.InteractionType = EInteractionType::Use;
	OutData.ActionText = GetInteractText_Implementation();
	OutData.DetailText = FText::FromString(TEXT("Any boarded player can depart the train."));
}

void AReturnTrainPlatformActor::OnBoardingVolumeBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
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
		BoardedControllers.Add(Controller);
	}

	if (!bAutoStartReturnBoardingOnEntry)
	{
		return;
	}

	if (UTrainTravelComponent* TrainTravel = GetTrainTravelComponent())
	{
		if (TrainTravel->GetTrainState() == ETrainState::Idle)
		{
			if (UWorld* World = GetWorld())
			{
				if (UCoreLoopSubsystem* CoreLoop = World->GetSubsystem<UCoreLoopSubsystem>())
				{
					if (CoreLoop->IsRunActive() && CoreLoop->GetCurrentPhase() == ECoreLoopPhase::Underground)
					{
						TrainTravel->BeginReturnBoarding();
					}
				}
			}
		}
	}
}

void AReturnTrainPlatformActor::OnBoardingVolumeEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
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
		BoardedControllers.Remove(Controller);
	}
}

bool AReturnTrainPlatformActor::IsCharacterBoarded(const ACharacter* Character) const
{
	return Character && Character->GetController() && BoardedControllers.Contains(Character->GetController());
}

bool AReturnTrainPlatformActor::CanOperateDeparture(const ACharacter* Character) const
{
	if (!Character || !IsCharacterBoarded(Character))
	{
		return false;
	}

	return GetTrainTravelComponent() != nullptr;
}

UTrainTravelComponent* AReturnTrainPlatformActor::GetTrainTravelComponent() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	if (const APickpackerGameState* PickpackerGameState = World->GetGameState<APickpackerGameState>())
	{
		return PickpackerGameState->GetTrainTravelComponent();
	}

	return nullptr;
}
