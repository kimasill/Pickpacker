// Fill out your copyright notice in the Description page of Project Settings.


#include "Library/DynamicGameplayStatics.h"

UObject* UDynamicGameplayStatics::GetActorOrComponentWithInterface(AActor* InActor, TSubclassOf<UInterface> InterfaceClass)
{
	if (!InActor || !InterfaceClass)
	{
		return nullptr;
	}

    if (InActor->GetClass()->ImplementsInterface(InterfaceClass))
    {
        return InActor;
    }

    TArray<UActorComponent*> Components = InActor->GetComponentsByInterface(InterfaceClass);
    if (Components.Num() > 0)
    {
        return Components[0];
    }
    return nullptr;
}
