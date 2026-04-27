#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ReturnToBaseVolume.generated.h"

class UBoxComponent;

UCLASS()
class PICKPACKER_API AReturnToBaseVolume : public AActor
{
	GENERATED_BODY()

public:
	AReturnToBaseVolume();

	virtual void BeginPlay() override;

protected:
	UFUNCTION()
	void OnTriggerBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void EvaluateReturnCondition();
	void TriggerReturnToBase();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> TriggerVolume;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Return")
	bool bRequireAllPlayersInside = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Return")
	bool bTravelToBaseLevel = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Return")
	TSoftObjectPtr<UWorld> BaseLevel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Return")
	bool bResetTrainState = true;

private:
	UPROPERTY()
	TSet<TWeakObjectPtr<AController>> ControllersInside;

	UPROPERTY()
	bool bReturnTriggered = false;
};
