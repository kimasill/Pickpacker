#include "Spawning/StationSpawnMarker.h"

TSubclassOf<AActor> AStationSpawnMarker::ChooseClassForSpawn() const
{
    // For now, use base class random pick behavior
    return Super::ChooseClassForSpawn();
}


