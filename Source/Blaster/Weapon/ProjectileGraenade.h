// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Projectile.h"
#include "ProjectileGraenade.generated.h"

/**
 * 
 */
UCLASS()
class BLASTER_API AProjectileGraenade : public AProjectile
{
	GENERATED_BODY()
public:
	AProjectileGraenade();
	virtual void Destroyed() override;
protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity);

private:
	UPROPERTY(EditAnywhere)
	USoundCue* BounceSound;
};
