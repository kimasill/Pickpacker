#include "MotherPatrolZoneActor.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"


AMotherPatrolZoneActor::AMotherPatrolZoneActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	PatrolVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("PatrolVolume"));
	SetRootComponent(PatrolVolume);
	PatrolVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PatrolVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	PatrolVolume->SetBoxExtent(BoxExtent);
	PatrolVolume->SetGenerateOverlapEvents(false);

	// Mother의 태그 기반 검색에 의해 자동 발견됨
	Tags.AddUnique(FName("MotherInspectionPoint"));
}

void AMotherPatrolZoneActor::BeginPlay()
{
	Super::BeginPlay();

	if (PatrolVolume)
	{
		PatrolVolume->SetBoxExtent(BoxExtent);
	}
}

#if WITH_EDITOR
void AMotherPatrolZoneActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropName = PropertyChangedEvent.GetPropertyName();
	if (PropName == GET_MEMBER_NAME_CHECKED(AMotherPatrolZoneActor, BoxExtent) && PatrolVolume)
	{
		PatrolVolume->SetBoxExtent(BoxExtent);
	}
}
#endif

FVector AMotherPatrolZoneActor::GetZoneCenter() const
{
	return PatrolVolume ? PatrolVolume->GetComponentLocation() : GetActorLocation();
}

FVector AMotherPatrolZoneActor::GetZoneExtent() const
{
	return PatrolVolume ? PatrolVolume->GetScaledBoxExtent() : BoxExtent;
}
