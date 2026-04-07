// Fill out your copyright notice in the Description page of Project Settings.


#include "Projectile.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundCue.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Character/BlasterCharacter.h"
#include "PlayerController/BlasterPlayerController.h"
#include "Pickpacker.h"
#include "Weapon/Weapon.h"
#include "Engine/OverlapResult.h"
#include "BlasterComponents/LagCompensationComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "TimerManager.h"
#include "GameFramework/DamageType.h"



AProjectile::AProjectile()
{ 	
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true; // Enable replication for network play
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	SetRootComponent(CollisionBox);
	CollisionBox->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
	CollisionBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldStatic, ECollisionResponse::ECR_Block);
	CollisionBox->SetCollisionResponseToChannel(ECC_SkeletalMesh, ECollisionResponse::ECR_Block);
	
}

void AProjectile::BeginPlay()
{
	Super::BeginPlay();
	
	/*if (Tracer)
	{
		TracerComponent = UGameplayStatics::SpawnEmitterAttached(
			Tracer, 
			CollisionBox, 
			FName(), 
			GetActorLocation(),
			GetActorRotation(),
			EAttachLocation::KeepWorldPosition
		);
	}*/

	if (Tracer)
	{
		TracerComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			Tracer,
			CollisionBox,
			FName(),
			GetActorLocation(),
			GetActorRotation(),
			EAttachLocation::KeepWorldPosition,
			false // Auto Destroy
		);
	}

	if (GetOwner())
	{
		CollisionBox->IgnoreActorWhenMoving(GetOwner(), true);
	}
	if (GetInstigator())
	{
		CollisionBox->IgnoreActorWhenMoving(GetInstigator(), true);
	}

	if(HasAuthority())
	{
		CollisionBox->OnComponentHit.AddDynamic(this, &AProjectile::OnHit);
	}
}

void AProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{	
	Destroy(); // Destroy the projectile after impact
}

void AProjectile::SpawnTrailSystem()
{
	if (TrailSystem)
	{
		TrailSystemComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			TrailSystem, // Niagara system to spawn
			GetRootComponent(), // Attach to the root component
			FName(), // Attach point name
			GetActorLocation(), // Location
			GetActorRotation(), // Rotation
			EAttachLocation::KeepWorldPosition, // Attach location
			false // Auto destroy
		);
	}
}


void AProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}


void AProjectile::StartDestroyTimer()
{
	GetWorldTimerManager().SetTimer(
		DestroyTimer,
		this,
		&AProjectile::DestroyTimerFinished,
		DestroyTime
	);
}

void AProjectile::DestroyTimerFinished()
{
	Destroy();
}

void AProjectile::ExplodeDamage()
{
	APawn* FiringPawn = GetInstigator();
	if (FiringPawn)
	{
		AController* FiringController = FiringPawn->GetController();
		ABlasterCharacter* OwnerCharacter = Cast<ABlasterCharacter>(FiringPawn);		
		if (FiringController)
		{
			if (OwnerCharacter && OwnerCharacter->HasAuthority() && !bUseServerSideRewind)
			{
				UGameplayStatics::ApplyRadialDamageWithFalloff(
					this, // World context object
					OwningWeapon->GetDamage() + Damage,
					MinimumDamage, // Minimum damage					
					GetActorLocation(), // Origin of the damage
					DamageInnerRadius, // Inner radius
					DamageOuterRadius, // Outer radius
					DamageFalloff, // Damage falloff
					UDamageType::StaticClass(), // Damage type class
					TArray<AActor*>(), // Ignore actors
					this, // Damage causer
					FiringController, // Instigated by controller
					ECollisionChannel::ECC_Visibility // Collision channel
				);
			}
			if(OwnerCharacter && !OwnerCharacter->HasAuthority() && OwnerCharacter->IsLocallyControlled() && bUseServerSideRewind)
			{
				ABlasterPlayerController* OwnerController = Cast<ABlasterPlayerController>(OwnerCharacter->Controller);

				TArray<FOverlapResult> OverlapResults;
				TArray<ABlasterCharacter*> HitCharacters;
				FCollisionObjectQueryParams ObjectQueryParams;
				ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel1);
				FCollisionShape SphereShape = FCollisionShape::MakeSphere(DamageOuterRadius);
				GetWorld()->OverlapMultiByObjectType(
					OverlapResults, // Array to fill with overlapping actors
					GetActorLocation(), // Center of the overlap
					FQuat::Identity, // No rotation
					ObjectQueryParams, // Object types to check
					SphereShape // Sphere shape for overlap
				);

				for (const FOverlapResult& Result : OverlapResults)
				{
					// FOverlapResult���� ���� ������ �����ɴϴ�.
					AActor* OverlappedActor = Result.GetActor();
					ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(OverlappedActor);

					if (BlasterCharacter && !HitCharacters.Contains(BlasterCharacter))                        
					{
						HitCharacters.Add(BlasterCharacter);
					}
				}				
				if (OwnerCharacter->GetLagCompensation() && HitCharacters.Num() > 0)
				{
					OwnerCharacter->GetLagCompensation()->ServerExplosiveScoreRequest(
						HitCharacters,
						GetActorLocation(),
						OwnerController->GetServerTime() - OwnerController->SingleTripTime,
						UDamageType::StaticClass(),
						this // Damage Causer
					);
				}
			}
		}
	}
}

FExplosiveInfo AProjectile::GetExplosiveInfo() const
{
	FExplosiveInfo Info;
	Info.OuterDamage = MinimumDamage;
	Info.InnerDamage = Damage;
	Info.InnerRadius = DamageInnerRadius;
	Info.OuterRadius = DamageOuterRadius;
	Info.Falloff = DamageFalloff;
	return Info;
}

void AProjectile::Destroyed()
{
	Super::Destroyed();

	if (ImpactParticles)
	{		
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ImpactParticles, GetActorLocation(), GetActorRotation());
	}
	if (ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, GetActorLocation());
	}
}

