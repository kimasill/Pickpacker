// Copyright

#pragma once

#include "CoreMinimal.h"
#include "Spawning/SpawnMarkerBase.h"
#include "DataAssets/DA_ParcelData.h"
#include "Components/BoxComponent.h"
#include "ParcelSpawnMarker.generated.h"

class AParcelActor;

USTRUCT(BlueprintType)
struct FParcelSpawnCandidate
{
	GENERATED_BODY()

	/** ParcelDataAsset 내 Row 이름 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParcelSpawn", meta = (GetOptions = "GetParcelRowOptions"))
	FName ParcelRow;

	/** 해당 Row의 스폰 가중치 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParcelSpawn", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	/** 해당 Row가 선택되었을 때 한번에 스폰할 개수 범위 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParcelSpawn")
	FIntPoint SpawnCountRange = FIntPoint(1, 1);
};

/**
 * Spawn marker specialized for parcels. Supports continuous respawn.
 */
UCLASS(BlueprintType, Blueprintable)
class PICKPACKER_API AParcelSpawnMarker : public ASpawnMarkerBase
{
    GENERATED_BODY()

public:
    AParcelSpawnMarker();

    virtual void BeginPlay() override;

	/** 랜덤 스폰 범위 (여러 개 동시 스폰 시 사용) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ParcelSpawn")
	UBoxComponent* SpawnArea = nullptr;

	/** 스폰에 사용할 파슬 데이터 에셋 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParcelSpawn")
	UDA_ParcelData* ParcelDataAsset = nullptr;

	/** 후보군: ParcelData 내 Row + 가중치(비면 전체 랜덤) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParcelSpawn")
	TArray<FParcelSpawnCandidate> CandidateParcelRows;

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

    /** Spawn interval in seconds. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParcelSpawn")
    float RespawnDelay = 10.0f;

	/** 후보군이 비어 있을 때 사용할 기본 스폰 개수 범위 (예: 1~3) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ParcelSpawn")
	FIntPoint SpawnCountRange = FIntPoint(1, 1);

    

protected:
    virtual void OnSpawned(AActor* SpawnedActor) override;

private:
	AActor* SpawnParcel(FName Row, const FTransform& SpawnTransform);
	bool ChooseParcelCandidate(FName& OutRow, FIntPoint& OutRange) const;
	FTransform GetSpawnTransform(bool bRandomize) const;

    UPROPERTY()
    TArray<TWeakObjectPtr<AActor>> SpawnedActors;

    UPROPERTY()
    FTimerHandle RespawnTimerHandle;

    UFUNCTION()
    void HandleSpawnedDestroyed(AActor* DestroyedActor);

    void TryRespawn();
};


