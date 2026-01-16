// Fill out your copyright notice in the Description page of Project Settings.

#include "PackagingStationActor.h"
#include "Blaster/Parcel/ParcelActor.h"
#include "Blaster/Parcel/PackedParcelActor.h"
#include "Blaster/Parcel/UnpackedParcelActor.h"
#include "Blaster/DataAssets/DA_ParcelData.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/Components/InteractionComponent.h"

APackagingStationActor::APackagingStationActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// Create components
	StationMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StationMesh"));
	RootComponent = StationMesh;

	// 입력 영역
	InputArea = CreateDefaultSubobject<UBoxComponent>(TEXT("InputArea"));
	InputArea->SetupAttachment(RootComponent);
	InputArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InputArea->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	InputArea->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	InputArea->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldDynamic, ECollisionResponse::ECR_Overlap);
	InputArea->SetCollisionResponseToChannel(ECollisionChannel::ECC_PhysicsBody, ECollisionResponse::ECR_Overlap);

	// 출력 위치
	OutputLocation = CreateDefaultSubobject<USceneComponent>(TEXT("OutputLocation"));
	OutputLocation->SetupAttachment(RootComponent);

	// 기본 설정
	CurrentMode = EPackagingMode::Pack;
	ProcessingTime = 2.0f;
	bAutoProcess = true;
	ProcessingInterval = 1.0f;
	bIsProcessing = false;
	bEnableDebugLogging = true;
}

void APackagingStationActor::BeginPlay()
{
	Super::BeginPlay();

	// 오버랩 이벤트 설정
	if (InputArea)
	{
		InputArea->OnComponentBeginOverlap.AddDynamic(this, &APackagingStationActor::OnInputAreaOverlap);
	}

	// 자동 처리 타이머 설정
	if (bAutoProcess && HasAuthority())
	{
		GetWorldTimerManager().SetTimer(
			AutoProcessTimerHandle,
			this,
			&APackagingStationActor::ProcessInputArea,
			ProcessingInterval,
			true
		);
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PackagingStationActor] Packaging station initialized: %s (Mode: %s)"), 
			*GetName(), 
			CurrentMode == EPackagingMode::Pack ? TEXT("Pack") : TEXT("Unpack"));
	}
}

void APackagingStationActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APackagingStationActor::ToggleMode()
{
	SetMode(CurrentMode == EPackagingMode::Pack ? EPackagingMode::Unpack : EPackagingMode::Pack);
}

void APackagingStationActor::SetMode(EPackagingMode NewMode)
{
	if (CurrentMode == NewMode)
	{
		return;
	}

	CurrentMode = NewMode;

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PackagingStationActor] Mode changed to: %s"),
			CurrentMode == EPackagingMode::Pack ? TEXT("Pack") : TEXT("Unpack"));
	}

	OnModeChanged.Broadcast(CurrentMode);
}

void APackagingStationActor::ProcessInputArea()
{
	if (!HasAuthority() || bIsProcessing)
	{
		return;
	}

	if (CurrentMode == EPackagingMode::Pack)
	{
		ProcessPackaging();
	}
	else
	{
		ProcessUnpackaging();
	}
}

void APackagingStationActor::PackageParcel(AParcelActor* Parcel)
{
	if (!Parcel || !HasAuthority())
	{
		return;
	}

	if (Parcel->IsPackaged())
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PackagingStationActor] Parcel already packaged: %s"), *Parcel->GetName());
		}
		return;
	}

	// 포장 처리
	Parcel->SetPackaged(true);

	// 출력 위치로 이동
	if (OutputLocation)
	{
		FTransform OutputTransform = OutputLocation->GetComponentTransform();
		Parcel->SetActorTransform(OutputTransform);
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PackagingStationActor] Parcel packaged: %s"), *Parcel->GetName());
	}

	OnParcelPackaged.Broadcast(Parcel);
}

void APackagingStationActor::UnpackageParcel(AParcelActor* Parcel)
{
	if (!Parcel || !HasAuthority())
	{
		return;
	}

	if (!Parcel->IsPackaged())
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PackagingStationActor] Parcel not packaged: %s"), *Parcel->GetName());
		}
		return;
	}

	// 포장 해제 처리
	Parcel->SetPackaged(false);

	// 출력 위치로 이동
	if (OutputLocation)
	{
		FTransform OutputTransform = OutputLocation->GetComponentTransform();
		Parcel->SetActorTransform(OutputTransform);
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PackagingStationActor] Parcel unpackaged: %s"), *Parcel->GetName());
	}

	OnParcelUnpackaged.Broadcast(Parcel);
}

void APackagingStationActor::OnInputAreaOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	// 오버랩 이벤트는 자동 처리 타이머에서 처리
	// 필요시 여기서 즉시 처리할 수도 있음
}

AParcelActor* APackagingStationActor::FindParcelInArea() const
{
	if (!InputArea)
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// 오버랩된 액터 찾기
	TArray<AActor*> OverlappingActors;
	InputArea->GetOverlappingActors(OverlappingActors, AParcelActor::StaticClass());

	for (AActor* Actor : OverlappingActors)
	{
		AParcelActor* Parcel = Cast<AParcelActor>(Actor);
		if (Parcel && !Parcel->IsAttached())
		{
			return Parcel;
		}
	}

	return nullptr;
}

TArray<AParcelActor*> APackagingStationActor::CollectParcelsInArea() const
{
	TArray<AParcelActor*> Result;
	if (!InputArea)
	{
		return Result;
	}

	TArray<AActor*> Overlaps;
	InputArea->GetOverlappingActors(Overlaps, AParcelActor::StaticClass());
	for (AActor* Actor : Overlaps)
	{
		if (AParcelActor* Parcel = Cast<AParcelActor>(Actor))
		{
			if (!Parcel->IsAttached())
			{
				Result.Add(Parcel);
			}
		}
	}
	return Result;
}

void APackagingStationActor::ProcessPackaging()
{
	if (!ParcelDataAsset)
	{
		return;
	}

	const TArray<AParcelActor*> Parcels = CollectParcelsInArea();
	if (Parcels.Num() == 0)
	{
		return;
	}

	// 태그별로 그룹화
	TMap<FGameplayTag, TArray<AParcelActor*>> ByTag;
	TMap<FName, TArray<AParcelActor*>> ByRowName;
	for (AParcelActor* Parcel : Parcels)
	{
		if (!Parcel || Parcel->IsPackaged())
		{
			continue;
		}

		const FName ParcelRowName = Parcel->GetParcelDefinitionRowName();
		if (ParcelRowName != NAME_None)
		{
			ByRowName.FindOrAdd(ParcelRowName).Add(Parcel);
		}

		FGameplayTagContainer ParcelTagsContainer = Parcel->GetParcelTags();
		for (const FParcelPackageRecipe& Recipe : ParcelDataAsset->PackageRecipes)
		{
			if (Recipe.TargetParcelTag.IsValid() && ParcelTagsContainer.HasTag(Recipe.TargetParcelTag))
			{
				ByTag.FindOrAdd(Recipe.TargetParcelTag).Add(Parcel);
			}
		}
	}

	// 레시피 순회 후 조건 충족 시 포장 생성
	for (const FParcelPackageRecipe& Recipe : ParcelDataAsset->PackageRecipes)
	{
		if (Recipe.RequiredCount <= 0)
		{
			continue;
		}

		const bool bUseRowFilter = Recipe.TargetParcelRowName != NAME_None;
		if (!bUseRowFilter && !Recipe.TargetParcelTag.IsValid())
		{
			continue;
		}

		TArray<AParcelActor*>* CandidatesPtr = bUseRowFilter
			? ByRowName.Find(Recipe.TargetParcelRowName)
			: ByTag.Find(Recipe.TargetParcelTag);
		if (!CandidatesPtr || CandidatesPtr->Num() < Recipe.RequiredCount)
		{
			continue;
		}

		// 소비할 파슬 선택
		TArray<AParcelActor*> Consume;
		for (int32 i = 0; i < Recipe.RequiredCount; ++i)
		{
			Consume.Add((*CandidatesPtr)[i]);
		}

		// 출력 위치
		FTransform OutTransform = OutputLocation ? OutputLocation->GetComponentTransform() : GetActorTransform();

		// 포장 액터 스폰 (PackedParcelActor 사용)
		TSubclassOf<AParcelActor> UseClass = Recipe.PackagedClass
			? Recipe.PackagedClass
			: (DefaultPackagedClass ? DefaultPackagedClass : TSubclassOf<AParcelActor>(APackedParcelActor::StaticClass()));
		if (!UseClass)
		{
			continue;
		}

		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		AParcelActor* Packaged = GetWorld()->SpawnActor<AParcelActor>(UseClass, OutTransform, Params);
		if (Packaged)
		{
			// 실제 소비된 파슬을 기반으로 콘텐츠 자동 생성 (RowName 우선)
			TMap<FName, int32> ContentCounts;
			for (AParcelActor* P : Consume)
			{
				if (!P)
				{
					continue;
				}
				FName ContentRow = P->GetParcelDefinitionRowName();
				if (ContentRow == NAME_None && bUseRowFilter)
				{
					ContentRow = Recipe.TargetParcelRowName;
				}
				if (ContentRow != NAME_None)
				{
					ContentCounts.FindOrAdd(ContentRow)++;
				}
			}

			TArray<FParcelPackageContent> AutoContents;
			for (const TPair<FName, int32>& Pair : ContentCounts)
			{
				FParcelPackageContent Content;
				Content.ParcelRowName = Pair.Key;
				Content.Count = Pair.Value;
				AutoContents.Add(Content);
			}

			Packaged->SetParcelDataAsset(ParcelDataAsset);
			
			// 레시피 정보 설정 (모든 타입에 공통)
			Packaged->SetPackageRecipe(&Recipe, ParcelDataAsset);
			
			// PackedParcelActor인 경우 추가 설정
			if (APackedParcelActor* PackedParcel = Cast<APackedParcelActor>(Packaged))
			{
				// 레시피 RowName 설정 (TargetParcelRowName 또는 TagName 사용)
				FName RecipeRowName = Recipe.TargetParcelRowName != NAME_None 
					? Recipe.TargetParcelRowName 
					: Recipe.TargetParcelTag.GetTagName();
				if (RecipeRowName != NAME_None)
				{
					PackedParcel->SetPackageRecipeRowName(RecipeRowName);
				}
				
				// 콘텐츠 설정
				if (AutoContents.Num() > 0)
				{
					PackedParcel->SetInitialContents(AutoContents);
				}
			}
			else
			{
				// 일반 ParcelActor인 경우 (레거시)
				if (AutoContents.Num() > 0)
				{
					Packaged->SetPackageContents(AutoContents);
				}
			}
			
			// PackedParcelActor는 이미 포장 상태로 생성되므로 SetPackaged 불필요
			if (bEnableDebugLogging)
			{
				UE_LOG(LogTemp, Log, TEXT("[PackagingStationActor] Packaged bundle spawned: %s (Row: %s / Tag: %s)"),
					*Packaged->GetName(),
					*Recipe.TargetParcelRowName.ToString(),
					*Recipe.TargetParcelTag.ToString());
			}

			// 소비 파슬 제거
			for (AParcelActor* P : Consume)
			{
				if (P && !P->IsPendingKillPending())
				{
					P->Destroy();
				}
			}
			
			OnParcelPackaged.Broadcast(Packaged);
		}
	}
}

void APackagingStationActor::ProcessUnpackaging()
{
	const TArray<AParcelActor*> Parcels = CollectParcelsInArea();
	if (Parcels.Num() == 0)
	{
		return;
	}

	const FTransform OutTransform = OutputLocation ? OutputLocation->GetComponentTransform() : GetActorTransform();

	for (AParcelActor* Parcel : Parcels)
	{
		if (!Parcel || !Parcel->IsPackaged())
		{
			continue;
		}

		if (Parcel->IsPackageBundle())
		{
			Parcel->UnpackAtTransform(OutTransform, false);
		}
		else
		{
			Parcel->SetPackaged(false);
			Parcel->SetActorTransform(OutTransform);
			OnParcelUnpackaged.Broadcast(Parcel);
		}
	}
}

void APackagingStationActor::AddToProcessingQueue(AParcelActor* Parcel)
{
	if (!Parcel || bIsProcessing)
	{
		return;
	}

	// 이미 큐에 있는지 확인
	for (const TWeakObjectPtr<AParcelActor>& QueuedParcel : ProcessingQueue)
	{
		if (QueuedParcel.Get() == Parcel)
		{
			return; // 이미 큐에 있음
		}
	}

	ProcessingQueue.Add(Parcel);

	// 처리 중이 아니면 즉시 처리 시작
	if (!bIsProcessing)
	{
		ProcessNextInQueue();
	}
}

void APackagingStationActor::ProcessNextInQueue()
{
	if (bIsProcessing || ProcessingQueue.Num() == 0)
	{
		return;
	}

	// 유효하지 않은 Parcel 제거
	ProcessingQueue.RemoveAll([](const TWeakObjectPtr<AParcelActor>& Parcel)
	{
		return !Parcel.IsValid() || Parcel->IsAttached();
	});

	if (ProcessingQueue.Num() == 0)
	{
		return;
	}

	// 첫 번째 Parcel 처리
	CurrentProcessingParcel = ProcessingQueue[0];
	ProcessingQueue.RemoveAt(0);

	if (!CurrentProcessingParcel.IsValid())
	{
		// 유효하지 않으면 다음 처리
		ProcessNextInQueue();
		return;
	}

	bIsProcessing = true;

	// 처리 시간 후 완료
	if (GetWorld())
	{
		GetWorldTimerManager().SetTimer(
			ProcessingTimerHandle,
			this,
			&APackagingStationActor::OnProcessingComplete,
			ProcessingTime,
			false
		);
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PackagingStationActor] Processing started: %s (Mode: %s)"),
			*CurrentProcessingParcel->GetName(),
			CurrentMode == EPackagingMode::Pack ? TEXT("Pack") : TEXT("Unpack"));
	}
}

void APackagingStationActor::OnProcessingComplete()
{
	if (!CurrentProcessingParcel.IsValid())
	{
		bIsProcessing = false;
		ProcessNextInQueue();
		return;
	}

	AParcelActor* Parcel = CurrentProcessingParcel.Get();

	// 모드에 따라 처리
	if (CurrentMode == EPackagingMode::Pack)
	{
		PackageParcel(Parcel);
	}
	else
	{
		UnpackageParcel(Parcel);
	}

	bIsProcessing = false;
	CurrentProcessingParcel = nullptr;

	// 다음 처리
	ProcessNextInQueue();
}
