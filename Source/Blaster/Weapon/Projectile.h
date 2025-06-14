// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Projectile.generated.h"

UCLASS()
class BLASTER_API AProjectile : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AProjectile();
	virtual void Tick(float DeltaTime) override;
	virtual void Destroyed() override;
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UFUNCTION()	
	virtual void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

private:
	UPROPERTY(EditAnywhere)
	class UBoxComponent* CollisionBox; // Collision box for the projectile

	UPROPERTY(VisibleAnywhere)
	class UProjectileMovementComponent* ProjectileMovementComponent; // Movement component for the projectile

	UPROPERTY(EditAnywhere)
	class UNiagaraSystem* Tracer;

	class UNiagaraComponent* TracerComponent; // Component for the tracer particle system

	UPROPERTY(EditAnywhere)
	UParticleSystem* ImpactParticles; // Particle system for the impact effect

	UPROPERTY(EditAnywhere)
	class USoundCue* ImpactSound; // Sound effect for the impact
public:	
	

};
