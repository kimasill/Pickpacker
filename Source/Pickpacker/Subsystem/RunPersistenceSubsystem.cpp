#include "Subsystem/RunPersistenceSubsystem.h"

void URunPersistenceSubsystem::SaveSnapshot(const FRunState& RunState, const FTrainDestination& CurrentDestination, const FRouteSelectionResult& RouteSelectionResult, const TArray<FWorldFlagEntry>& WorldFlags, const TArray<FStorageRecord>& CargoRecords)
{
	TravelSnapshot = FPickpackerTravelSnapshot();
	TravelSnapshot.bValid = true;
	TravelSnapshot.RunState = RunState;
	TravelSnapshot.CurrentDestination = CurrentDestination;
	TravelSnapshot.RouteSelectionResult = RouteSelectionResult;
	TravelSnapshot.WorldFlags = WorldFlags;
	TravelSnapshot.CargoRecords = CargoRecords;
}

bool URunPersistenceSubsystem::ConsumeSnapshot(FPickpackerTravelSnapshot& OutSnapshot)
{
	if (!TravelSnapshot.bValid)
	{
		return false;
	}

	OutSnapshot = TravelSnapshot;
	ClearSnapshot();
	return true;
}

void URunPersistenceSubsystem::ClearSnapshot()
{
	TravelSnapshot = FPickpackerTravelSnapshot();
}
