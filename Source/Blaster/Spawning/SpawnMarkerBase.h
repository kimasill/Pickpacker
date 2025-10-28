// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpawnMarkerBase.generated.h"

/**
 * Base class for spawn markers that can spawn from candidate classes.
 * Server-only spawning; supports spawn on begin play and manual triggering.
 */
UCLASS(BlueprintType, Blueprintable)
class BLASTER_API ASpawnMarkerBase : public AActor
{
    GENERATED_BODY()

public:
    ASpawnMarkerBase();

    virtual void BeginPlay() override;

    /** Spawn one actor from candidates. Server only. Returns spawned actor or nullptr. */
    UFUNCTION(BlueprintCallable, Category = "SpawnMarker")
    AActor* SpawnOne();

    /** Despawn all spawned actors owned by this marker (does not affect directly placed actors). */
    UFUNCTION(BlueprintCallable, Category = "SpawnMarker")
    void DespawnAll();

    /** Returns last spawned actor (if any). */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SpawnMarker")
    AActor* GetLastSpawned() const { return LastSpawned.Get(); }

protected:
    /** Candidate actor classes to spawn from. Must be replicated actors for multiplayer. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpawnMarker")
    TArray<TSubclassOf<AActor>> CandidateClasses;

    /** If true, spawn immediately on BeginPlay (server only). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpawnMarker")
    bool bSpawnOnBeginPlay = true;

    /** If set, attach spawned actor to this marker using this socket (optional). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpawnMarker")
    FName AttachSocketName = NAME_None;

    /** Keep reference to spawned actor to manage lifecycle. */
    UPROPERTY()
    TWeakObjectPtr<AActor> LastSpawned;

    /** Hook to filter/adjust chosen class before spawn (override in derived). */
    virtual TSubclassOf<AActor> ChooseClassForSpawn() const;

    /** Called after successful spawn (override in derived). */
    virtual void OnSpawned(AActor* SpawnedActor) {}
};


