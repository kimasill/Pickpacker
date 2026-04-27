// Travel-safe snapshot storage for the Pickpacker core loop.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PickpackerTypes/CoreLoopTypes.h"
#include "Components/EscapeProgressComponent.h"
#include "RunPersistenceSubsystem.generated.h"

USTRUCT(BlueprintType)
struct PICKPACKER_API FPickpackerTravelSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickpacker|Travel")
	bool bValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickpacker|Travel")
	FRunState RunState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickpacker|Travel")
	FTrainDestination CurrentDestination;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickpacker|Travel")
	FRouteSelectionResult RouteSelectionResult;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickpacker|Travel")
	TArray<FWorldFlagEntry> WorldFlags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickpacker|Travel")
	TArray<FStorageRecord> CargoRecords;
};

UCLASS()
class PICKPACKER_API URunPersistenceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Pickpacker|Travel")
	void SaveSnapshot(const FRunState& RunState, const FTrainDestination& CurrentDestination, const FRouteSelectionResult& RouteSelectionResult, const TArray<FWorldFlagEntry>& WorldFlags, const TArray<FStorageRecord>& CargoRecords);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|Travel")
	bool HasSnapshot() const { return TravelSnapshot.bValid; }

	UFUNCTION(BlueprintCallable, Category = "Pickpacker|Travel")
	bool ConsumeSnapshot(FPickpackerTravelSnapshot& OutSnapshot);

	UFUNCTION(BlueprintCallable, Category = "Pickpacker|Travel")
	void ClearSnapshot();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pickpacker|Travel")
	const FPickpackerTravelSnapshot& GetSnapshot() const { return TravelSnapshot; }

private:
	UPROPERTY()
	FPickpackerTravelSnapshot TravelSnapshot;
};
