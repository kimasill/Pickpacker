#include "Blaster/Environment/ConveyorBeltActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/Parcel/ParcelActor.h"

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
	ConveyorVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	DropVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("DropVolume"));
	DropVolume->SetupAttachment(RootComponent);
	DropVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DropVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	DropVolume->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	DropVolume->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	DropVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
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

	if (ConveyorVolume)
	{
		TArray<UPrimitiveComponent*> OverlappingComponents;
		ConveyorVolume->GetOverlappingComponents(OverlappingComponents);
		for (UPrimitiveComponent* Component : OverlappingComponents)
		{
			if (!Component)
			{
				continue;
			}

			AParcelActor* Parcel = Cast<AParcelActor>(Component->GetOwner());
			if (!Parcel)
			{
				continue;
			}

			AddPrimitiveToConveyor(Component);
		}
	}
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

	if (OtherActor && CanConveyActor(OtherActor))
	{
		AddActorToConveyor(OtherActor);
	}
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

	if (OtherActor)
	{
		ReleaseActor(OtherActor);
	}
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

	if (OtherActor)
	{
		ReleaseActor(OtherActor);
	}
}

void AConveyorBeltActor::AddPrimitiveToConveyor(UPrimitiveComponent* Primitive)
{
	if (!Primitive || FindPrimitiveIndex(Primitive) != INDEX_NONE)
	{
		return;
	}

	if (ABlasterCharacter* Character = Cast<ABlasterCharacter>(Primitive->GetOwner()))
	{
		// 캐릭터는 물리 컨베이어가 아닌 Actor 컨베이어로 처리
		return;
	}

	if (AParcelActor* Parcel = Cast<AParcelActor>(Primitive->GetOwner()))
	{
		if (Parcel->IsAttached())
		{
			return;
		}
	}

	if (!Primitive->IsSimulatingPhysics())
	{
		Primitive->SetSimulatePhysics(true);
		Primitive->WakeAllRigidBodies();
	}

	if (!Primitive->IsSimulatingPhysics())
	{
		return;
	}

	FConveyedPrimitiveEntry Entry;
	Entry.Primitive = Primitive;

	if (AParcelActor* Parcel = Cast<AParcelActor>(Primitive->GetOwner()))
	{
		Parcel->SetOnConveyor(true);
		const FRotator CurrentRotation = Primitive->GetComponentRotation();
		Primitive->SetWorldRotation(FRotator(0.0f, CurrentRotation.Yaw, 0.0f));
		Primitive->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}

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

	if (Primitive)
	{
		if (AParcelActor* Parcel = Cast<AParcelActor>(Primitive->GetOwner()))
		{
			Parcel->SetOnConveyor(false);
		}
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

		if (AParcelActor* Parcel = Cast<AParcelActor>(Primitive->GetOwner()))
		{
			const FRotator CurrentRotation = Primitive->GetComponentRotation();
			if (!FMath::IsNearlyZero(CurrentRotation.Roll) || !FMath::IsNearlyZero(CurrentRotation.Pitch))
			{
				Primitive->SetWorldRotation(FRotator(0.0f, CurrentRotation.Yaw, 0.0f));
			}
			Primitive->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		}
	}

	for (int32 i = ConveyedActors.Num() - 1; i >= 0; --i)
	{
		if (!ConveyedActors[i].IsValid())
		{
			ConveyedActors.RemoveAtSwap(i);
			continue;
		}

		AActor* Actor = ConveyedActors[i].Get();
		if (!Actor || !CanConveyActor(Actor))
		{
			ConveyedActors.RemoveAtSwap(i);
			continue;
		}

		Actor->AddActorWorldOffset(Delta, true);
	}
}

FVector AConveyorBeltActor::GetConveyorEntryLocation() const
{
	if (ConveyorVolume)
	{
		return ConveyorVolume->GetComponentLocation() + EntryOffset;
	}
	return GetActorLocation() + EntryOffset;
}

bool AConveyorBeltActor::CanConveyActor(AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}

	if (ABlasterCharacter* Character = Cast<ABlasterCharacter>(Actor))
	{
		return Character->IsOutOfLives();
	}

	if (Cast<AParcelActor>(Actor) != nullptr)
	{
		return false;
	}

	return true;
}

void AConveyorBeltActor::AddActorToConveyor(AActor* Actor)
{
	if (!Actor || FindActorIndex(Actor) != INDEX_NONE)
	{
		return;
	}

	ConveyedActors.Add(Actor);
}

void AConveyorBeltActor::ReleaseActor(AActor* Actor)
{
	const int32 Index = FindActorIndex(Actor);
	if (Index == INDEX_NONE)
	{
		return;
	}

	ConveyedActors.RemoveAtSwap(Index);
}

int32 AConveyorBeltActor::FindActorIndex(AActor* Actor) const
{
	for (int32 i = 0; i < ConveyedActors.Num(); ++i)
	{
		if (ConveyedActors[i].Get() == Actor)
		{
			return i;
		}
	}

	return INDEX_NONE;
}

