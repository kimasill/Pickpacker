#include "Escape/TrainCargoLoaderActor.h"

#include "Character/BlasterCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/InteractionComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TrainTravelComponent.h"
#include "GameState/PickpackerGameState.h"
#include "Parcel/ParcelActor.h"
#include "Subsystem/CoreLoopSubsystem.h"
#include "Engine/World.h"

ATrainCargoLoaderActor::ATrainCargoLoaderActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	InteractionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionVolume"));
	InteractionVolume->SetupAttachment(SceneRoot);
	InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionVolume->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Block);
	InteractionVolume->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractionVolume->SetBoxExtent(FVector(80.0f, 80.0f, 110.0f));
}

void ATrainCargoLoaderActor::OnInteract_Implementation(ACharacter* Interactor)
{
	if (!HasAuthority() || !CanInteract_Implementation(Interactor))
	{
		return;
	}

	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(Interactor);
	if (!BlasterCharacter)
	{
		return;
	}

	UInteractionComponent* InteractionComponent = BlasterCharacter->GetInteractionComponent();
	UTrainTravelComponent* TrainTravel = GetTrainTravelComponent();
	AParcelActor* Parcel = GetCarriedParcel(Interactor);
	if (!InteractionComponent || !TrainTravel || !Parcel)
	{
		return;
	}

	if (!TrainTravel->LoadParcelIntoCargo(Parcel))
	{
		return;
	}

	InteractionComponent->SetCarriedParcel(nullptr);
	Parcel->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Parcel->Destroy();
}

bool ATrainCargoLoaderActor::CanInteract_Implementation(ACharacter* Interactor)
{
	if (!Interactor || !GetCarriedParcel(Interactor))
	{
		return false;
	}

	if (!HasAuthority())
	{
		return true;
	}

	const UWorld* World = GetWorld();
	const UTrainTravelComponent* TrainTravel = GetTrainTravelComponent();
	if (!World || !TrainTravel)
	{
		return false;
	}

	const UCoreLoopSubsystem* CoreLoop = World->GetSubsystem<UCoreLoopSubsystem>();
	if (!CoreLoop || !CoreLoop->IsRunActive() || CoreLoop->GetCurrentPhase() != ECoreLoopPhase::Underground)
	{
		return false;
	}

	return TrainTravel->GetTrainState() == ETrainState::Idle ||
		(TrainTravel->GetJourneyType() == ETrainJourneyType::ReturnToBase &&
		 TrainTravel->GetTrainState() == ETrainState::Boarding);
}

FText ATrainCargoLoaderActor::GetInteractText_Implementation()
{
	return FText::FromString(TEXT("Load Parcel Into Train Cargo"));
}

bool ATrainCargoLoaderActor::RequestShowInteractionUI_Implementation(ACharacter* Interactor)
{
	return false;
}

void ATrainCargoLoaderActor::GetInteractionUIData_Implementation(FInteractionUIData& OutData)
{
	OutData.InteractionType = EInteractionType::Use;
	OutData.ActionText = GetInteractText_Implementation();
	OutData.DetailText = FText::FromString(TEXT("Transfers the carried parcel into the return train cargo."));
}

AParcelActor* ATrainCargoLoaderActor::GetCarriedParcel(ACharacter* Interactor) const
{
	const ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(Interactor);
	if (!BlasterCharacter)
	{
		return nullptr;
	}

	const UInteractionComponent* InteractionComponent = BlasterCharacter->GetInteractionComponent();
	return InteractionComponent ? InteractionComponent->GetCarriedParcel() : nullptr;
}

UTrainTravelComponent* ATrainCargoLoaderActor::GetTrainTravelComponent() const
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
