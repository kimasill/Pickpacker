#include "Spawning/ParcelSpawnMarker.h"
#include "Parcel/ParcelActor.h"
#include "Parcel/UnpackedParcelActor.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Components/BoxComponent.h"

AParcelSpawnMarker::AParcelSpawnMarker()
{
    MaxSimultaneous = 1;
    RespawnDelay = 10.0f;
	ParcelActorClass = AParcelActor::StaticClass();

	SpawnArea = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnArea"));
	SpawnArea->SetupAttachment(RootComponent);
	SpawnArea->SetBoxExtent(FVector(100.0f, 100.0f, 50.0f));
	SpawnArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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

	const int32 Remaining = MaxSimultaneous - Alive;
	if (Remaining <= 0)
	{
		return;
	}

	FName SelectedRow = NAME_None;
	FIntPoint SelectedRange(1, 1);
	if (!ChooseParcelCandidate(SelectedRow, SelectedRange) || SelectedRow == NAME_None)
	{
		return;
	}

	const int32 RangeMin = FMath::Max(1, FMath::Min(SelectedRange.X, SelectedRange.Y));
	const int32 RangeMax = FMath::Max(RangeMin, FMath::Max(SelectedRange.X, SelectedRange.Y));
	const int32 SpawnCount = FMath::Min(FMath::RandRange(RangeMin, RangeMax), Remaining);
	const bool bRandomizeLocation = SpawnCount > 1;

    for (int32 SpawnIndex = 0; SpawnIndex < SpawnCount; ++SpawnIndex)
    {
		const FTransform SpawnTransform = GetSpawnTransform(bRandomizeLocation);
        AActor* Spawned = SpawnParcel(SelectedRow, SpawnTransform);
        if (!Spawned)
        {
            break;
        }
    }
}

void AParcelSpawnMarker::StartSpawning()
{
	if (!HasAuthority())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimerHandle);
	}

	// 즉시 채우기
	TryRespawn();

	if (!bRespawnEnabled)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		const float Interval = FMath::Max(0.01f, RespawnDelay);
		World->GetTimerManager().SetTimer(RespawnTimerHandle, this, &AParcelSpawnMarker::TryRespawn, Interval, true);
	}
}

AActor* AParcelSpawnMarker::SpawnParcel(FName Row, const FTransform& SpawnTransform)
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

	// 기본적으로 UnpackedParcelActor 사용
	if (!ParcelActorClass)
	{
		ParcelActorClass = AUnpackedParcelActor::StaticClass();
	}

	AParcelActor* Parcel = World->SpawnActorDeferred<AParcelActor>(
		ParcelActorClass,
		SpawnTransform,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn
	);
	if (!Parcel)
	{
		return nullptr;
	}

	// 데이터 에셋 + RowName 주입 후 초기화 (BeginPlay 이전)
	if (ParcelDataAsset)
	{
		Parcel->SetParcelDataAsset(ParcelDataAsset);
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
	Parcel->FinishSpawning(SpawnTransform);

	OnSpawned(Parcel);
	return Parcel;
}

FTransform AParcelSpawnMarker::GetSpawnTransform(bool bRandomize) const
{
	if (!bRandomize || !SpawnArea)
	{
		return GetActorTransform();
	}

	const FVector Extent = SpawnArea->GetScaledBoxExtent();
	const FVector LocalPoint(
		FMath::RandRange(-Extent.X, Extent.X),
		FMath::RandRange(-Extent.Y, Extent.Y),
		FMath::RandRange(-Extent.Z, Extent.Z)
	);
	const FVector WorldPoint = SpawnArea->GetComponentTransform().TransformPosition(LocalPoint);

	FTransform Result = GetActorTransform();
	Result.SetLocation(WorldPoint);
	return Result;
}

bool AParcelSpawnMarker::ChooseParcelCandidate(FName& OutRow, FIntPoint& OutRange) const
{
	OutRow = NAME_None;
	OutRange = SpawnCountRange;

	if (CandidateParcelRows.Num() > 0)
	{
		float TotalWeight = 0.0f;
		for (const FParcelSpawnCandidate& Candidate : CandidateParcelRows)
		{
			if (Candidate.ParcelRow != NAME_None && Candidate.Weight > 0.0f)
			{
				TotalWeight += Candidate.Weight;
			}
		}

		if (TotalWeight > 0.0f)
		{
			const float Pick = FMath::FRandRange(0.0f, TotalWeight);
			float Accumulated = 0.0f;
			for (const FParcelSpawnCandidate& Candidate : CandidateParcelRows)
			{
				if (Candidate.ParcelRow == NAME_None || Candidate.Weight <= 0.0f)
				{
					continue;
				}
				Accumulated += Candidate.Weight;
				if (Pick <= Accumulated)
				{
					OutRow = Candidate.ParcelRow;
					OutRange = Candidate.SpawnCountRange;
					return true;
				}
			}
		}

		// 가중치가 없거나 모두 0인 경우 균등 랜덤
		TArray<FName> ValidRows;
		for (const FParcelSpawnCandidate& Candidate : CandidateParcelRows)
		{
			if (Candidate.ParcelRow != NAME_None)
			{
				ValidRows.Add(Candidate.ParcelRow);
			}
		}
		if (ValidRows.Num() > 0)
		{
			const int32 Index = FMath::RandRange(0, ValidRows.Num() - 1);
			const FName PickedRow = ValidRows[Index];
			for (const FParcelSpawnCandidate& Candidate : CandidateParcelRows)
			{
				if (Candidate.ParcelRow == PickedRow)
				{
					OutRow = Candidate.ParcelRow;
					OutRange = Candidate.SpawnCountRange;
					return true;
				}
			}
			OutRow = PickedRow;
			return true;
		}
	}

	if (ParcelDataAsset && ParcelDataAsset->ParcelConfigs.Num() > 0)
	{
		const int32 Index = FMath::RandRange(0, ParcelDataAsset->ParcelConfigs.Num() - 1);
		OutRow = FName(*FString::FromInt(Index)); // DA_ParcelData GetParcelConfigByName supports numeric index
		return true;
	}

	return false;
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


