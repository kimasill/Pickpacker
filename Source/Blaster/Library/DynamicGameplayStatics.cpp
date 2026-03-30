// Fill out your copyright notice in the Description page of Project Settings.


#include "Library/DynamicGameplayStatics.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/Interface.h"
#include "Components/ActorComponent.h"
#include "Components/ChildActorComponent.h"

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

void UDynamicGameplayStatics::GetActorAndChildObjectsWithInterface(AActor* InActor, TSubclassOf<UInterface> InterfaceClass, TArray<UObject*>& OutObjects)
{
    OutObjects.Reset();

    if (InActor == nullptr || !InterfaceClass)
    {
        return;
    }

    auto AddObjectsFromActor = [&OutObjects, InterfaceClass](AActor* Actor)
    {
        if (!Actor)
        {
            return;
        }

        if (Actor->GetClass()->ImplementsInterface(InterfaceClass))
        {
            OutObjects.AddUnique(Cast<UObject>(Actor));
        }

        TArray<UActorComponent*> Components = Actor->GetComponentsByInterface(InterfaceClass);
        for (UActorComponent* Comp : Components)
        {
            if (Comp)
            {
                OutObjects.AddUnique(Cast<UObject>(Comp));
            }
        }
    };

    TArray<AActor*> PendingActors;
    PendingActors.Add(InActor);

    for (int32 Index = 0; Index < PendingActors.Num(); ++Index)
    {
        AActor* Actor = PendingActors[Index];
        if (!Actor)
        {
            continue;
        }

        AddObjectsFromActor(Actor);

        TArray<UChildActorComponent*> ChildComponents;
        Actor->GetComponents<UChildActorComponent>(ChildComponents);
        for (UChildActorComponent* ChildComp : ChildComponents)
        {
            if (!ChildComp)
            {
                continue;
            }

            AActor* ChildActor = ChildComp->GetChildActor();
            if (!ChildActor)
            {
                continue;
            }

            PendingActors.Add(ChildActor);
        }
    }
}
