#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "ElevatorPlatformActor.generated.h"

class UBoxComponent;

UCLASS()
class BLASTER_API AElevatorPlatformActor : public AActor
{
	GENERATED_BODY()
	
public:	
	AElevatorPlatformActor();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTriggerBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void EvaluateAllPlayersInside();

	class UEscapeProgressComponent* GetEscapeProgress() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Elevator")
	TObjectPtr<UBoxComponent> TriggerVolume;

	/** 모든 플레이어 탑승 시 설정할 월드 플래그 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Elevator")
	FGameplayTag AllPlayersInsideFlag;

private:
	TSet<TWeakObjectPtr<AController>> ControllersInside;
};



























