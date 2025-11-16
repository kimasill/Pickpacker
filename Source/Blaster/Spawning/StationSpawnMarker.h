// Copyright

#pragma once

#include "CoreMinimal.h"
#include "Spawning/SpawnMarkerBase.h"
#include "StationSpawnMarker.generated.h"

/**
 * Spawn marker specialized for stations. Chooses from Station candidate list.
 */
UCLASS(BlueprintType, Blueprintable)
class BLASTER_API AStationSpawnMarker : public ASpawnMarkerBase
{
    GENERATED_BODY()

protected:
    virtual TSubclassOf<AActor> ChooseClassForSpawn() const override;
};


