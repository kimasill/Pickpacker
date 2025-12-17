// Fill out your copyright notice in the Description page of Project Settings.


#include "Library/DynamicGameplayStatics.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/Interface.h"
#include "Components/ActorComponent.h"

UObject* UDynamicGameplayStatics::GetActorOrComponentWithInterface(AActor* InActor, TSubclassOf<UInterface> InterfaceClass)
{
    if (InActor == nullptr || !InterfaceClass)
    {
        return nullptr;
    }

    // If actor implements the interface, return it
    if (InActor->GetClass()->ImplementsInterface(InterfaceClass))
    {
        return Cast<UObject>(InActor);
    }

    // Search components implementing the interface
    TArray<UActorComponent*> Components = InActor->GetComponentsByInterface(InterfaceClass);
    for (UActorComponent* Comp : Components)
    {
        if (Comp)
        {
            return Cast<UObject>(Comp);
        }
    }

    return nullptr;
}
