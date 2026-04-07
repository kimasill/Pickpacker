#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SubmissionZoneActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USceneComponent;

/**
 * Submission zone placed at the end of the delivery belt.
 * Detects parcels that reach the zone and notifies the game mode.
 */
UCLASS(BlueprintType, Blueprintable)
class PICKPACKER_API ASubmissionZoneActor : public AActor
{
	GENERATED_BODY()

public:
	ASubmissionZoneActor();

	virtual void BeginPlay() override;

protected:
	UFUNCTION()
	void HandleSubmissionOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* ZoneMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* SubmissionArea;

public:
	/** Should only packaged parcels be accepted */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submission")
	bool bRequiresPackagedParcel = true;

	/** 사망한 플레이어 도착 시 관전 전환 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submission")
	bool bHandleDeadPlayers = true;

	/** Optional debug logging */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Submission")
	bool bEnableDebugLogging = false;
};

