#include "Spawning/ParcelSpawnMarker.h"
#include "Parcel/ParcelActor.h"
#include "Parcel/UnpackedParcelActor.h"
#include "Engine/World.h"
#include "TimerManager.h"

AParcelSpawnMarker::AParcelSpawnMarker()
{
    MaxSimultaneous = 1;
    RespawnDelay = 10.0f;
	ParcelActorClass = AParcelActor::StaticClass();
}

void AParcelSpawnMarker::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority() && bSpawnOnBeginPlay)
    {
		StartSpawning();
    }
}

void AParcelSpawnMarker::OnSpawned(AActor* SpawnedActor)
{
    if (!SpawnedActor)
    {
        return;
    }
    SpawnedActors.Add(SpawnedActor);
    SpawnedActor->OnDestroyed.AddDynamic(this, &AParcelSpawnMarker::HandleSpawnedDestroyed);


}

void AParcelSpawnMarker::HandleSpawnedDestroyed(AActor* DestroyedActor)
{
    SpawnedActors.RemoveAllSwap([DestroyedActor](const TWeakObjectPtr<AActor>& Ptr)
    {
        return !Ptr.IsValid() || Ptr.Get() == DestroyedActor;
    });

    if (HasAuthority())
    {
        if (!bRespawnEnabled)
        {
            return;
		}
        GetWorld()->GetTimerManager().SetTimer(RespawnTimerHandle, this, &AParcelSpawnMarker::TryRespawn, RespawnDelay, false);
    }
}

void AParcelSpawnMarker::TryRespawn()
{
    if (!HasAuthority())
    {
        return;
    }

    int32 Alive = 0;
    for (const TWeakObjectPtr<AActor>& Ptr : SpawnedActors)
    {
        if (Ptr.IsValid())
        {
            ++Alive;
        }
    }
    while (Alive < MaxSimultaneous)
    {
        AActor* Spawned = SpawnParcel();
        if (!Spawned)
        {
            break;
        }
        ++Alive;
    }
}

void AParcelSpawnMarker::StartSpawning()
{
	if (!HasAuthority())
	{
		return;
	}

	// 즉시 채우기
	TryRespawn();
}

AActor* AParcelSpawnMarker::SpawnParcel()
{
	if (!HasAuthority())
	{
		return nullptr;
	}

	if (!ParcelActorClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ParcelSpawnMarker] ParcelActorClass not set on %s"), *GetName());
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const FTransform SpawnTransform = GetActorTransform();
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// 기본적으로 UnpackedParcelActor 사용
	if (!ParcelActorClass)
	{
		ParcelActorClass = AUnpackedParcelActor::StaticClass();
	}

	AParcelActor* Parcel = World->SpawnActor<AParcelActor>(ParcelActorClass, SpawnTransform, Params);
	if (!Parcel)
	{
		return nullptr;
	}

	// 데이터 에셋 + RowName 주입 후 초기화
	if (ParcelDataAsset)
	{
		Parcel->SetParcelDataAsset(ParcelDataAsset);
		const FName Row = ChooseParcelRow();
		if (Row != NAME_None)
		{
			Parcel->SetParcelDefinitionRowName(Row);
			// UnpackedParcelActor는 BeginPlay에서 자동 초기화됨
			if (!Parcel->IsA<AUnpackedParcelActor>())
			{
				Parcel->ApplyParcelConfigFromDataAsset(true);
			}
		}
	}

	OnSpawned(Parcel);
	return Parcel;
}

FName AParcelSpawnMarker::ChooseParcelRow() const
{
	if (CandidateParcelRows.Num() > 0)
	{
		const int32 Index = FMath::RandRange(0, CandidateParcelRows.Num() - 1);
		return CandidateParcelRows[Index];
	}

	if (ParcelDataAsset && ParcelDataAsset->ParcelConfigs.Num() > 0)
	{
		const int32 Index = FMath::RandRange(0, ParcelDataAsset->ParcelConfigs.Num() - 1);
		return FName(*FString::FromInt(Index)); // DA_ParcelData GetParcelConfigByName supports numeric index
	}

	return NAME_None;
}

TArray<FName> AParcelSpawnMarker::GetParcelRowOptions() const
{
	TArray<FName> Options;
	if (!ParcelDataAsset)
	{
		return Options;
	}

	const int32 Num = ParcelDataAsset->ParcelConfigs.Num();
	Options.Reserve(Num);
	for (int32 Index = 0; Index < Num; ++Index)
	{
		const FParcelConfig& Config = ParcelDataAsset->ParcelConfigs[Index];
		FString Label = Config.ParcelName;
		if (Label.IsEmpty())
		{
			Label = Config.ParcelTag.IsValid()
				? Config.ParcelTag.ToString()
				: FString::Printf(TEXT("Parcel_%d"), Index);
		}
		Options.Add(FName(*Label));
	}
	return Options;
}


