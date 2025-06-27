// Fill out your copyright notice in the Description page of Project Settings.

#include "HitScanWeapon.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundCue.h"
#include "Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"
#include "WeaponTypes.h"

void AHitScanWeapon::Fire(const FVector& HitTarget)
{
	Super::Fire(HitTarget);
	APawn* OwnerPawn = GetOwner<APawn>();
	if (OwnerPawn == nullptr) return;
	AController* InstigatorController = OwnerPawn->GetController();

	const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName("MuzzleFlash");
	if (MuzzleFlashSocket)
	{
		FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
		FVector Start = SocketTransform.GetLocation();

		FHitResult FireHit;
		WeaponTraceHit(Start, HitTarget, FireHit);		
		
		ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(FireHit.GetActor());
		if (BlasterCharacter && HasAuthority() && InstigatorController)
		{
			UGameplayStatics::ApplyDamage(
				BlasterCharacter,
				Damage,
				InstigatorController,
				this,
				UDamageType::StaticClass()
			);
		}
		if (ImpactParticles)
		{
			UGameplayStatics::SpawnEmitterAtLocation(
				GetWorld(),
				ImpactParticles,
				FireHit.ImpactPoint,
				FireHit.ImpactNormal.Rotation()
			);
		}
		if (ImpactSound)
		{
			UGameplayStatics::PlaySoundAtLocation(
				this,
				ImpactSound,
				FireHit.ImpactPoint
			);
		}
		
		if (MuzzleFlash)
		{
			UGameplayStatics::SpawnEmitterAtLocation(
				GetWorld(),
				MuzzleFlash,
				SocketTransform
			);
		}

		if (FireSound)
		{
			UGameplayStatics::PlaySoundAtLocation(
				this,
				FireSound,
				GetActorLocation()
			);
		}				
	}
}

FVector AHitScanWeapon::TraceEndWithScatter(const FVector& TractStart, const FVector& HitTarget) const
{
	FVector ToTargetNormalized = (HitTarget - TractStart).GetSafeNormal();
	FVector SphereCenter = TractStart + ToTargetNormalized * DistanceToSphere;
	FVector RandVec = UKismetMathLibrary::RandomUnitVector() * FMath::FRandRange(0.0f, SphereRadius);
	FVector EndLoc = SphereCenter + RandVec;
	FVector ToEndLoc = EndLoc - TractStart;
	
	/**
	DrawDebugSphere(
		GetWorld(),
		SphereCenter,
		SphereRadius,
		12,
		FColor::Red,
		false,
		2.0f
	);
	DrawDebugSphere(
		GetWorld(),
		EndLoc,
		4.f,
		12,
		FColor::Orange,
		true
	);
	DrawDebugLine(
		GetWorld(),
		TractStart,
		FVector(TractStart + ToEndLoc * TRACE_LENGTH / ToEndLoc.Size()),
		FColor::Cyan,
		true
	);
	**/
	return FVector(TractStart + ToEndLoc * TRACE_LENGTH / ToEndLoc.Size());
}

void AHitScanWeapon::WeaponTraceHit(const FVector& TraceStart, const FVector& HitTarget, FHitResult& OutHit)
{
	FHitResult FireHit;
	UWorld* World = GetWorld();
	if (World) {
		FVector End = bUseScatter ? TraceEndWithScatter(TraceStart, HitTarget) : TraceStart + (HitTarget - TraceStart) * 1.25f;
		World->LineTraceSingleByChannel(
			OutHit,
			TraceStart,
			End,
			ECC_Visibility
		);
		FVector BeamEnd = End;
		if (OutHit.bBlockingHit)
		{
			BeamEnd = OutHit.ImpactPoint;
		}	
		if (BeamParticles)
		{
			UParticleSystemComponent* Beam = UGameplayStatics::SpawnEmitterAtLocation(
				World,
				BeamParticles,
				TraceStart, // Start location
				FRotator::ZeroRotator, // No rotation needed for the beam start
				true
			);
			if (Beam)
			{
				Beam->SetVectorParameter("Target", BeamEnd); // Set the end point of the beam
			}
		}
	}
	
}
