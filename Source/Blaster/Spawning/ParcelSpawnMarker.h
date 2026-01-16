// Copyright

#pragma once

#include "CoreMinimal.h"
#include "Spawning/SpawnMarkerBase.h"
#include "Blaster/DataAssets/DA_ParcelData.h"
#include "ParcelSpawnMarker.generated.h"

class AParcelActor;

/**
 * Spawn marker specialized for parcels. Supports continuous respawn.
 */
UCLASS(BlueprintType, Blueprintable)
class BLASTER_API AParcelSpawnMarker : public ASpawnMarkerBase
{
    GENERATED_BODY()

public:
    AParcelSpawnMarker();

    virtual void BeginPlay() override;

	/** 스폰에 사용할 파슬 데이터 에셋 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParcelSpawn")
	UDA_ParcelData* ParcelDataAsset = nullptr;

	/** 후보군: ParcelData 내 Row 이름(비면 전체 랜덤) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParcelSpawn", meta = (GetOptions = "GetParcelRowOptions"))
	TArray<FName> CandidateParcelRows;

	/** 스폰할 파슬 클래스 (기본 AParcelActor) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParcelSpawn")
	TSubclassOf<AParcelActor> ParcelActorClass;

	/** 현재 ParcelDataAsset에 있는 Row 이름 목록을 반환 (에디터 드롭다운용) */
	UFUNCTION()
	TArray<FName> GetParcelRowOptions() const;

	/** 스폰 루프 시작 (Blueprint에서 호출 가능) */
	UFUNCTION(BlueprintCallable, Category = "ParcelSpawn")
	void StartSpawning();

    /** Max simultaneous spawned actors maintained by this marker. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParcelSpawn")
    int32 MaxSimultaneous = 1;

	/** Whether respawning is enabled after an actor is destroyed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParcelSpawn")
	bool bRespawnEnabled = true;

    /** Respawn delay after an actor is destroyed (seconds). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParcelSpawn")
    float RespawnDelay = 10.0f;

    

protected:
    virtual void OnSpawned(AActor* SpawnedActor) override;

private:
	AActor* SpawnParcel();
	FName ChooseParcelRow() const;

    UPROPERTY()
    TArray<TWeakObjectPtr<AActor>> SpawnedActors;

    UPROPERTY()
    FTimerHandle RespawnTimerHandle;

    UFUNCTION()
    void HandleSpawnedDestroyed(AActor* DestroyedActor);

    void TryRespawn();
};


