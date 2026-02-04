// Copyright

#include "Blaster/Environment/LadderActor.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Blaster/Character/BlasterCharacter.h"

ALadderActor::ALadderActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	TriggerVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerVolume"));
	TriggerVolume->SetupAttachment(Root);
	TriggerVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerVolume->SetGenerateOverlapEvents(true);
	TriggerVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerVolume->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	TriggerVolume->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	TriggerVolume->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
	TriggerVolume->SetGenerateOverlapEvents(true);

	BottomPoint = CreateDefaultSubobject<USceneComponent>(TEXT("BottomPoint"));
	BottomPoint->SetupAttachment(Root);

	TopPoint = CreateDefaultSubobject<USceneComponent>(TEXT("TopPoint"));
	TopPoint->SetupAttachment(Root);
}

void ALadderActor::BeginPlay()
{
	Super::BeginPlay();

	if (!bIgnorePawnCollision)
	{
		return;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* Component : PrimitiveComponents)
	{
		if (!Component || Component == TriggerVolume)
		{
			continue;
		}

		Component->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		if (Component->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
		{
			continue;
		}
	}
}

bool ALadderActor::TryStartClimb(ACharacter* Interactor)
{
	if (!bCanAccess || !Interactor)
	{
		return false;
	}

	if (ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(Interactor))
	{
		BlasterCharacter->StartLadder(this);
		return true;
	}

	return false;
}

void ALadderActor::ForceStopClimb(ACharacter* Interactor, bool bPlaceAtTop)
{
	if (ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(Interactor))
	{
		BlasterCharacter->StopLadder(bPlaceAtTop);
	}
}

FVector ALadderActor::GetBottomLocation() const
{
	return BottomPoint ? BottomPoint->GetComponentLocation() : GetActorLocation();
}

FVector ALadderActor::GetTopLocation() const
{
	return TopPoint ? TopPoint->GetComponentLocation() : GetActorLocation();
}

FVector ALadderActor::GetLadderUpVector() const
{
	const FVector Bottom = GetBottomLocation();
	const FVector Top = GetTopLocation();
	return (Top - Bottom).GetSafeNormal();
}

float ALadderActor::GetLadderLength() const
{
	return FVector::Dist(GetBottomLocation(), GetTopLocation());
}
