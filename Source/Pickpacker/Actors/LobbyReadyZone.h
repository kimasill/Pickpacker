#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerBox.h"
#include "LobbyReadyZone.generated.h"

/**
 * 로비 준비 구역: 구역에 들어오면 자동 준비, 나가면 준비 해제
 */
UCLASS()
class BLASTER_API ALobbyReadyZone : public ATriggerBox
{
	GENERATED_BODY()

public:
	ALobbyReadyZone();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnZoneBeginOverlap(AActor* OverlappedActor, AActor* OtherActor);

	UFUNCTION()
	void OnZoneEndOverlap(AActor* OverlappedActor, AActor* OtherActor);

private:
	void UpdateReadyState(AActor* OtherActor, bool bReady);
};


