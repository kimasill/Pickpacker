// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayActionInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UGameplayActionInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class BLASTER_API IGameplayActionInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "GameplayAction")
    void Action(class APawn* Owner);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "GameplayAction")
	void PreAction();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "GameplayAction")
	void Paint(FVector2D UVLocation);
};
