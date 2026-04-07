// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PickpackerSuspicionLibrary.generated.h"

/**
 * 
 */
UCLASS()
class PICKPACKER_API UPickpackerSuspicionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	

public:
	/**
	 * Check specific suspicious behavior
	 */
	UFUNCTION(BlueprintCallable, Category = "Drone")
	static ESuspiciousBehavior CheckSuspiciousBehavior(class ACharacter* Player);

};
