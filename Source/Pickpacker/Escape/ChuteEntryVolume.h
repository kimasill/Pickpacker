#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "ChuteEntryVolume.generated.h"

class UBoxComponent;

UCLASS()
class PICKPACKER_API AChuteEntryVolume : public AActor
{
	GENERATED_BODY()
	
public:	
	AChuteEntryVolume();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTriggerBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void EvaluateAllPlayersInside();

	class UEscapeProgressComponent* GetEscapeProgress() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chute")
	TObjectPtr<UBoxComponent> TriggerVolume;

	/** 모든 플레이어가 진입했을 때 설정할 월드 플래그 (선택) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chute")
	FGameplayTag AllPlayersInsideFlag;

private:
	TSet<TWeakObjectPtr<AController>> ControllersInside;
};



























