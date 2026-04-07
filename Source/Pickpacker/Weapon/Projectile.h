// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/ExplosiveSource.h"
#include "Weapon/Weapon.h"
#include "Projectile.generated.h"


UCLASS()
class PICKPACKER_API AProjectile : public AActor, public IExplosiveSource
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AProjectile();
	virtual void Tick(float DeltaTime) override;
	virtual void Destroyed() override;
	

	UPROPERTY()
	class AWeapon* OwningWeapon = nullptr; // Pointer to the weapon that fired this projectile

	/**
	* Used with server-side rewind
	*/

	bool bUseServerSideRewind = false;
	FVector_NetQuantize TraceStart;
	FVector_NetQuantize100 InitialVelocity;

	UPROPERTY(EditAnywhere)
	float InitialSpeed = 15000.f; // Initial speed of the projectile

	UPROPERTY(EditAnywhere)
	float Damage = 20.f;

	UPROPERTY(EditAnywhere)
	float HeadShotDamage = 40;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	void StartDestroyTimer();
	void DestroyTimerFinished();
	void ExplodeDamage();

	UFUNCTION()	
	virtual void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY(EditAnywhere)
	class UNiagaraSystem* ImpactParticles;

	UPROPERTY(EditAnywhere)
	class USoundCue* ImpactSound; // Sound effect for the impact

	UPROPERTY(EditAnywhere)
	class UBoxComponent* CollisionBox; // Collision box for the projectile

	UPROPERTY(EditAnywhere)
	class UNiagaraSystem* TrailSystem;

	UPROPERTY()
	class UNiagaraComponent* TrailSystemComponent;

	void SpawnTrailSystem();

	UPROPERTY(VisibleAnywhere)
	class UProjectileMovementComponent* ProjectileMovementComponent; // Movement component for the projectile

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* ProjectileMesh;

	UPROPERTY(EditAnywhere)
	float DamageInnerRadius = 200.f; // Inner radius for area damage

	UPROPERTY(EditAnywhere)
	float DamageOuterRadius = 500.f; // Outer radius for area damage

	UPROPERTY(EditAnywhere)
	float DamageFalloff = 1.f; // Falloff for damage based on distance

	UPROPERTY(EditAnywhere)
	float MinimumDamage = 10.f; // Minimum damage dealt to targets within the outer radius

private:

	UPROPERTY(EditAnywhere)
	UNiagaraSystem* Tracer;

	UPROPERTY()
	class UNiagaraComponent* TracerComponent; // Component for the tracer particle system


	FTimerHandle DestroyTimer;

	UPROPERTY(EditAnywhere)
	float DestroyTime = 3.f;

public:	
	virtual AWeapon* GetExplosiveCauser() const override { return OwningWeapon ? OwningWeapon : nullptr; }
	virtual FExplosiveInfo GetExplosiveInfo() const override;
	FORCEINLINE float GetDamage() const { return Damage; }
	FORCEINLINE float GetHeadShotDamage() const { return HeadShotDamage; }
};
