// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/Interface.h"
#include "Templates/SubclassOf.h"
#include "DynamicGameplayStatics.generated.h"

/**
 * 
 */
UCLASS()
class BLASTER_API UDynamicGameplayStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Helper")
	static UObject* GetActorOrComponentWithInterface(AActor* InActor, TSubclassOf<UInterface> InterfaceClass);
};
