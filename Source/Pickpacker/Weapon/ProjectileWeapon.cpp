// Fill out your copyright notice in the Description page of Project Settings.


#include "ProjectileWeapon.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Projectile.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"

void AProjectileWeapon::Fire(const FVector& HitTarget)
{
	Super::Fire(HitTarget);
	APawn* InstigatorPawn = Cast<APawn>(GetOwner());

	const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName(FName("MuzzleFlash"));


	UWorld* World = GetWorld();
	if (MuzzleFlashSocket && World)
	{
		FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());	
		// From MuzzleFlashSocket to HitTarget from TraceUnderCrosshair
		FVector ToTarget = HitTarget - SocketTransform.GetLocation(); // Calculate the direction to the target
		FRotator TargetRotation = ToTarget.Rotation(); // Convert the direction to a rotation

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GetOwner();
		SpawnParams.Instigator = InstigatorPawn;

		AProjectile* SpawnedProjectile = nullptr;
		if (bUseServerSideRewind)
		{
			if(InstigatorPawn->HasAuthority()) // server
			{
				if (InstigatorPawn->IsLocallyControlled()) // server, host - use replicated projectile
				{
					SpawnedProjectile = World->SpawnActor<AProjectile>(ProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
					SpawnedProjectile->bUseServerSideRewind = false; // Don't use server-side rewind for the projectile
					SpawnedProjectile->OwningWeapon = this; // Set the owning weapon for the projectile
				}
				else // server, not locally controlled - spawn non-replicated projectile
				{
					SpawnedProjectile = World->SpawnActor<AProjectile>(ServerSideRewindProjectileClass,SocketTransform.GetLocation(),TargetRotation, SpawnParams);
					SpawnedProjectile->bUseServerSideRewind = true; // Use server-side rewind for the projectile
					SpawnedProjectile->OwningWeapon = this;
				}
			}
			else
			{
				if (InstigatorPawn->IsLocallyControlled())// client, locally controlled - spawn non-replicated projectile, use ssr
				{ 
					SpawnedProjectile = World->SpawnActor<AProjectile>(ServerSideRewindProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
					SpawnedProjectile->bUseServerSideRewind = true;
					SpawnedProjectile->TraceStart = SocketTransform.GetLocation();
					SpawnedProjectile->InitialVelocity = SpawnedProjectile->GetActorForwardVector() * SpawnedProjectile->InitialSpeed;
					SpawnedProjectile->OwningWeapon = this; // Set the owning weapon for the projectile
				}
				else
				{
					SpawnedProjectile = World->SpawnActor<AProjectile>(ServerSideRewindProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
					SpawnedProjectile->bUseServerSideRewind = false; // Use server-side rewind for the projectile
					SpawnedProjectile->OwningWeapon = this;
				}
			}
		}
		else // weapon not using SSR
		{
			if(InstigatorPawn->HasAuthority())
			{
				SpawnedProjectile = World->SpawnActor<AProjectile>(ProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
				SpawnedProjectile->bUseServerSideRewind = false; // Don't use server-side rewind for the projectile
				SpawnedProjectile->bUseServerSideRewind = false;
			}
		}
	}
}
