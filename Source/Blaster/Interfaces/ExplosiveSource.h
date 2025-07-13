// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ExplosiveSource.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UExplosiveSource : public UInterface
{
	GENERATED_BODY()
};

class BLASTER_API IExplosiveSource
{
	GENERATED_BODY()	
public:
	virtual float GetExplosiveDamage() const = 0;
	virtual class AWeapon* GetExplosiveCauser() const = 0;
};
