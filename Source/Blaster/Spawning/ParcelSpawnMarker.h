// Copyright

#pragma once

#include "CoreMinimal.h"
#include "Spawning/SpawnMarkerBase.h"
#include "ParcelSpawnMarker.generated.h"

/**
 * Spawn marker specialized for parcels. Supports continuous respawn.
 */
UCLASS(BlueprintType, Blueprintable)
class BLASTER_API AParcelSpawnMarker : public ASpawnMarkerBase
{
    GENERATED_BODY()

public:
    AParcelSpawnMarker();

    virtual void BeginPlay() override;

    /** Max simultaneous spawned actors maintained by this marker. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParcelSpawn")
    int32 MaxSimultaneous = 1;

	/** Whether respawning is enabled after an actor is destroyed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParcelSpawn")
	bool bRespawnEnabled = true;

    /** Respawn delay after an actor is destroyed (seconds). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParcelSpawn")
    float RespawnDelay = 10.0f;

    

protected:
    virtual void OnSpawned(AActor* SpawnedActor) override;

private:
    UPROPERTY()
    TArray<TWeakObjectPtr<AActor>> SpawnedActors;

    UPROPERTY()
    FTimerHandle RespawnTimerHandle;

    UFUNCTION()
    void HandleSpawnedDestroyed(AActor* DestroyedActor);

    void TryRespawn();
};


