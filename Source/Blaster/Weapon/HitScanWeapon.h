// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapon.h"
#include "HitScanWeapon.generated.h"

/**
 * 
 */
UCLASS()
class BLASTER_API AHitScanWeapon : public AWeapon
{

	GENERATED_BODY()


public:
	virtual void Fire(const FVector& HitTarget) override;

protected:
	void WeaponTraceHit(const FVector& TraceStart, const FVector& HitTarget, FHitResult& OutHit);

	UPROPERTY(EditAnywhere)
	class UParticleSystem* ImpactParticles; // Effect to play on hit

	UPROPERTY(EditAnywhere)
	USoundCue* ImpactSound; // Sound to play on impact
private:
	
	UPROPERTY(EditAnywhere)
	UParticleSystem* BeamParticles; // Effect to play on beam trace

	UPROPERTY(EditAnywhere)
	UParticleSystem* MuzzleFlash; // Effect to play on muzzle flash

	UPROPERTY(EditAnywhere)
	USoundCue* FireSound; // Sound to play when firing

};
