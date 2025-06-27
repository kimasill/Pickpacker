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
	FVector TraceEndWithScatter(const FVector& TractStart, const FVector& HitTarget) const;
	void WeaponTraceHit(const FVector& TraceStart, const FVector& HitTarget, FHitResult& OutHit);

	UPROPERTY(EditAnywhere)
	class UParticleSystem* ImpactParticles; // Effect to play on hit

	UPROPERTY(EditAnywhere)
	USoundCue* ImpactSound; // Sound to play on impact

	UPROPERTY(EditAnywhere)
	float Damage = 20.f; // Amount of damage this weapon does

private:
	
	UPROPERTY(EditAnywhere)
	UParticleSystem* BeamParticles; // Effect to play on beam trace

	UPROPERTY(EditAnywhere)
	UParticleSystem* MuzzleFlash; // Effect to play on muzzle flash

	UPROPERTY(EditAnywhere)
	USoundCue* FireSound; // Sound to play when firing


	/**
	* Trace end with scatter
	*/

	UPROPERTY(EditAnywhere, Category = "Weapon Scatter")
	float DistanceToSphere = 800.f; // Distance to sphere for scatter effect

	UPROPERTY(EditAnywhere, Category = "Weapon Scatter")
	float SphereRadius = 75.f; // Radius of the sphere for scatter effect

	UPROPERTY(EditAnywhere, Category = "Weapon Scatter")
	bool bUseScatter = false; // Whether to use scatter effect or not
};
