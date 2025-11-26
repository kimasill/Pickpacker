#include "Blaster/Environment/ConveyorBeltActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"

AConveyorBeltActor::AConveyorBeltActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	BeltMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeltMesh"));
	BeltMesh->SetupAttachment(RootComponent);
	BeltMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	ConveyorVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("ConveyorVolume"));
	ConveyorVolume->SetupAttachment(RootComponent);
	ConveyorVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ConveyorVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	ConveyorVolume->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	ConveyorVolume->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	ConveyorVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	DropVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("DropVolume"));
	DropVolume->SetupAttachment(RootComponent);
	DropVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DropVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	DropVolume->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	DropVolume->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	DropVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
}

void AConveyorBeltActor::BeginPlay()
{
	Super::BeginPlay();

	if (ConveyorVolume)
	{
		ConveyorVolume->OnComponentBeginOverlap.AddDynamic(this, &AConveyorBeltActor::HandleConveyorBeginOverlap);
		ConveyorVolume->OnComponentEndOverlap.AddDynamic(this, &AConveyorBeltActor::HandleConveyorEndOverlap);
	}

	if (DropVolume)
	{
		DropVolume->OnComponentBeginOverlap.AddDynamic(this, &AConveyorBeltActor::HandleDropOverlap);
	}

	BeltDirection = BeltDirection.GetSafeNormal();
}

void AConveyorBeltActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
	{
		return;
	}

	UpdateConveyedPrimitives(DeltaSeconds);
}

void AConveyorBeltActor::HandleConveyorBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority() || !OtherComp)
	{
		return;
	}

	AddPrimitiveToConveyor(OtherComp);
}

void AConveyorBeltActor::HandleConveyorEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	if (!HasAuthority() || !OtherComp || !bReleaseOnVolumeExit)
	{
		return;
	}

	ReleasePrimitive(OtherComp, true);
}

void AConveyorBeltActor::HandleDropOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority() || !OtherComp)
	{
		return;
	}

	ReleasePrimitive(OtherComp, true);
}

void AConveyorBeltActor::AddPrimitiveToConveyor(UPrimitiveComponent* Primitive)
{
	if (!Primitive || FindPrimitiveIndex(Primitive) != INDEX_NONE)
	{
		return;
	}

	if (!Primitive->IsSimulatingPhysics())
	{
		return;
	}

	FConveyedPrimitiveEntry Entry;
	Entry.Primitive = Primitive;

	if (bDisableGravityWhileConveyed && Primitive->IsGravityEnabled())
	{
		Primitive->SetEnableGravity(false);
		Entry.bRestoreGravity = true;
	}

	Primitive->WakeAllRigidBodies();
	ConveyedPrimitives.Add(Entry);
}

void AConveyorBeltActor::ReleasePrimitive(UPrimitiveComponent* Primitive, bool bRestorePhysics)
{
	const int32 Index = FindPrimitiveIndex(Primitive);
	if (Index == INDEX_NONE)
	{
		return;
	}

	if (Primitive && bDisableGravityWhileConveyed && bRestorePhysics && ConveyedPrimitives[Index].bRestoreGravity)
	{
		Primitive->SetEnableGravity(true);
	}

	ConveyedPrimitives.RemoveAtSwap(Index);
}

int32 AConveyorBeltActor::FindPrimitiveIndex(UPrimitiveComponent* Primitive) const
{
	for (int32 i = 0; i < ConveyedPrimitives.Num(); ++i)
	{
		if (ConveyedPrimitives[i].Primitive == Primitive)
		{
			return i;
		}
	}

	return INDEX_NONE;
}

void AConveyorBeltActor::UpdateConveyedPrimitives(float DeltaSeconds)
{
	if (BeltDirection.IsNearlyZero() || BeltSpeed <= 0.f)
	{
		return;
	}

	const FVector Delta = BeltDirection * BeltSpeed * DeltaSeconds;

	for (int32 i = ConveyedPrimitives.Num() - 1; i >= 0; --i)
	{
		if (!ConveyedPrimitives[i].Primitive.IsValid())
		{
			ConveyedPrimitives.RemoveAtSwap(i);
			continue;
		}

		UPrimitiveComponent* Primitive = ConveyedPrimitives[i].Primitive.Get();
		Primitive->AddWorldOffset(Delta, true);
	}
}

