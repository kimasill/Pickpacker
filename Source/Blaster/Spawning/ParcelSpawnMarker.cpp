#include "Spawning/ParcelSpawnMarker.h"
#include "Engine/World.h"
#include "TimerManager.h"

AParcelSpawnMarker::AParcelSpawnMarker()
{
    MaxSimultaneous = 1;
    RespawnDelay = 10.0f;
}

void AParcelSpawnMarker::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority() && bSpawnOnBeginPlay)
    {
        int32 Alive = 0;
        for(const TWeakObjectPtr<AActor>& Ptr : SpawnedActors)
        {
            if (Ptr.IsValid())
            {
                ++Alive;
            }
		}
        while (Alive < MaxSimultaneous)
        {
            AActor* Spawned = SpawnOne();
            if (!Spawned)
            {
                break;
            }
            ++Alive;
        }
    }
}

void AParcelSpawnMarker::OnSpawned(AActor* SpawnedActor)
{
    if (!SpawnedActor)
    {
        return;
    }
    SpawnedActors.Add(SpawnedActor);
    SpawnedActor->OnDestroyed.AddDynamic(this, &AParcelSpawnMarker::HandleSpawnedDestroyed);


}

void AParcelSpawnMarker::HandleSpawnedDestroyed(AActor* DestroyedActor)
{
    SpawnedActors.RemoveAllSwap([DestroyedActor](const TWeakObjectPtr<AActor>& Ptr)
    {
        return !Ptr.IsValid() || Ptr.Get() == DestroyedActor;
    });

    if (HasAuthority())
    {
        if (!bRespawnEnabled)
        {
            return;
		}
        GetWorld()->GetTimerManager().SetTimer(RespawnTimerHandle, this, &AParcelSpawnMarker::TryRespawn, RespawnDelay, false);
    }
}

void AParcelSpawnMarker::TryRespawn()
{
    if (!HasAuthority())
    {
        return;
    }

    int32 Alive = 0;
    for (const TWeakObjectPtr<AActor>& Ptr : SpawnedActors)
    {
        if (Ptr.IsValid())
        {
            ++Alive;
        }
    }
    while (Alive < MaxSimultaneous)
    {
        AActor* Spawned = SpawnOne();
        if (!Spawned)
        {
            break;
        }
        ++Alive;
    }
}


