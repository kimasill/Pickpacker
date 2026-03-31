// Fill out your copyright notice in the Description page of Project Settings.

#include "PPPatrolBoundsLibrary.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"

bool PPPatrolBoundsLibrary::GetVolumeBoundsFromActor(AActor* VolumeActor, FVector& OutCenter, FVector& OutExtent)
{
	if (!VolumeActor)
	{
		return false;
	}

	if (UBoxComponent* BoxComp = VolumeActor->FindComponentByClass<UBoxComponent>())
	{
		OutCenter = BoxComp->GetComponentLocation();
		OutExtent = BoxComp->GetScaledBoxExtent();
		return true;
	}

	if (USphereComponent* SphereComp = VolumeActor->FindComponentByClass<USphereComponent>())
	{
		OutCenter = SphereComp->GetComponentLocation();
		const float Radius = SphereComp->GetScaledSphereRadius();
		OutExtent = FVector(Radius, Radius, Radius);
		return true;
	}

	FVector Origin;
	FVector BoxExtent;
	VolumeActor->GetActorBounds(false, Origin, BoxExtent);
	OutCenter = Origin;
	OutExtent = BoxExtent;
	return true;
}

FVector PPPatrolBoundsLibrary::GenerateRandomPatrolPointHorizontal(
	const FVector& Center,
	const FVector& Extent,
	float MinPatrolRadius,
	float MaxPatrolRadius)
{
	FVector RandomPoint;
	RandomPoint.X = FMath::RandRange(Center.X - Extent.X, Center.X + Extent.X);
	RandomPoint.Y = FMath::RandRange(Center.Y - Extent.Y, Center.Y + Extent.Y);
	RandomPoint.Z = Center.Z;

	FVector ToPoint = RandomPoint - Center;
	ToPoint.Z = 0.0f;
	float Distance = ToPoint.Size();

	if (Distance < MinPatrolRadius)
	{
		ToPoint = ToPoint.GetSafeNormal() * MinPatrolRadius;
		RandomPoint = Center + ToPoint;
		RandomPoint.Z = Center.Z;
	}
	else if (Distance > MaxPatrolRadius)
	{
		ToPoint = ToPoint.GetSafeNormal() * MaxPatrolRadius;
		RandomPoint = Center + ToPoint;
		RandomPoint.Z = Center.Z;
	}

	return RandomPoint;
}
