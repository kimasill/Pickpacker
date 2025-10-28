// Fill out your copyright notice in the Description page of Project Settings.

#include "Spawning/SpawnMarkerBase.h"
#include "Engine/World.h"

ASpawnMarkerBase::ASpawnMarkerBase()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true; // Marker itself can replicate for visibility; spawned actors should also replicate
}

void ASpawnMarkerBase::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority() && bSpawnOnBeginPlay)
    {
        SpawnOne();
    }
}

TSubclassOf<AActor> ASpawnMarkerBase::ChooseClassForSpawn() const
{
    if (CandidateClasses.Num() <= 0)
    {
        return nullptr;
    }
    const int32 Index = FMath::RandRange(0, CandidateClasses.Num() - 1);
    return CandidateClasses[Index];
}

AActor* ASpawnMarkerBase::SpawnOne()
{
    if (!HasAuthority())
    {
        return nullptr;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    TSubclassOf<AActor> ClassToSpawn = ChooseClassForSpawn();
    if (!*ClassToSpawn)
    {
        return nullptr;
    }

    const FTransform SpawnTransform = GetActorTransform();
    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    AActor* Spawned = World->SpawnActor<AActor>(*ClassToSpawn, SpawnTransform, Params);
    if (Spawned)
    {
        LastSpawned = Spawned;
        if (!AttachSocketName.IsNone())
        {
            Spawned->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform, AttachSocketName);
        }
        OnSpawned(Spawned);
    }

    return Spawned;
}

void ASpawnMarkerBase::DespawnAll()
{
    if (!HasAuthority())
    {
        return;
    }

    if (LastSpawned.IsValid())
    {
        LastSpawned->Destroy();
        LastSpawned = nullptr;
    }
}


