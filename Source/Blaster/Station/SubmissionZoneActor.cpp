#include "Blaster/Station/SubmissionZoneActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Blaster/Parcel/ParcelActor.h"
#include "Blaster/GameMode/PickpackerGameMode.h"

ASubmissionZoneActor::ASubmissionZoneActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	ZoneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ZoneMesh"));
	ZoneMesh->SetupAttachment(RootComponent);
	ZoneMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SubmissionArea = CreateDefaultSubobject<UBoxComponent>(TEXT("SubmissionArea"));
	SubmissionArea->SetupAttachment(RootComponent);
	SubmissionArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SubmissionArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	SubmissionArea->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	SubmissionArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void ASubmissionZoneActor::BeginPlay()
{
	Super::BeginPlay();

	if (SubmissionArea)
	{
		SubmissionArea->OnComponentBeginOverlap.AddDynamic(this, &ASubmissionZoneActor::HandleSubmissionOverlap);
	}
}

void ASubmissionZoneActor::HandleSubmissionOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	AParcelActor* Parcel = Cast<AParcelActor>(OtherActor);
	if (!Parcel)
	{
		return;
	}

	if (bRequiresPackagedParcel && !Parcel->IsPackaged())
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Warning, TEXT("[SubmissionZoneActor] Parcel rejected - not packaged (%s)"), *Parcel->GetName());
		}
		return;
	}

	APickpackerGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APickpackerGameMode>() : nullptr;
	if (!GameMode)
	{
		return;
	}

	GameMode->ReportParcelSubmitted(Parcel);

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[SubmissionZoneActor] Parcel submitted: %s"), *Parcel->GetName());
	}
}


