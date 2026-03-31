// Fill out your copyright notice in the Description page of Project Settings.

#include "PackagingStationActor.h"
#include "Blaster/Parcel/ParcelActor.h"
#include "Blaster/Parcel/PackedParcelActor.h"
#include "Blaster/Parcel/UnpackedParcelActor.h"
#include "Blaster/DataAssets/DA_ParcelData.h"
#include "Blaster/GameMode/PickpackerGameMode.h"
#include "Blaster/GameState/PickpackerGameState.h"
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
			ByRowName.FindOrAdd(ParcelRowName).AddUnique(Parcel);
		}

		FGameplayTagContainer ParcelTagsContainer = Parcel->GetParcelTags();
		for (const FParcelPackageRecipe& Recipe : ParcelDataAsset->PackageRecipes)
		{
			for (const FGameplayTag& Tag : Recipe.TargetParcelTags)
			{
				if (Tag.IsValid() && ParcelTagsContainer.HasTag(Tag))
				{
					ByTag.FindOrAdd(Tag).AddUnique(Parcel);
				}
			}
		}
	}

	// 레시피 순회 후 조건 충족 시 포장 생성
	auto GetCandidateUnits = [](const TArray<AParcelActor*>& Candidates)
	{
		int32 TotalUnits = 0;
		for (AParcelActor* Parcel : Candidates)
		{
			if (!Parcel)
			{
				continue;
			}
			TotalUnits += Parcel->GetPackagingSpaceUnits();
		}
		return TotalUnits;
	};

	TMap<int32, TArray<FName>> MatchedRowsByRecipe;
	TMap<int32, TArray<FGameplayTag>> MatchedTagsByRecipe;
	TSet<int32> EligibleRowRecipes;
	TSet<int32> EligibleTagRecipes;
	for (int32 RecipeIndex = 0; RecipeIndex < ParcelDataAsset->PackageRecipes.Num(); ++RecipeIndex)
	{
		const FParcelPackageRecipe& Recipe = ParcelDataAsset->PackageRecipes[RecipeIndex];
		const int32 MinUnits = Recipe.MinRequiredCount;
		const int32 MaxUnits = Recipe.MaxRequiredCount;
		const int32 MinContentCount = FMath::Max(1, Recipe.MinContentCount);
		if (MinUnits <= 0 || MaxUnits < MinUnits)
		{
			continue;
		}

		bool bHasRowMatch = false;
		for (const FName& RowTarget : Recipe.TargetParcelRowNames)
		{
			TArray<AParcelActor*>* CandidatesPtr = ByRowName.Find(RowTarget);
			const int32 TotalUnits = CandidatesPtr ? GetCandidateUnits(*CandidatesPtr) : 0;
			const int32 CandidateCount = CandidatesPtr ? CandidatesPtr->Num() : 0;
			if (CandidatesPtr && TotalUnits >= MinUnits && CandidateCount >= MinContentCount)
			{
				MatchedRowsByRecipe.FindOrAdd(RecipeIndex).Add(RowTarget);
				EligibleRowRecipes.Add(RecipeIndex);
				bHasRowMatch = true;
			}
		}

		if (bHasRowMatch)
		{
			continue;
		}

		for (const FGameplayTag& TagTarget : Recipe.TargetParcelTags)
		{
			if (!TagTarget.IsValid())
			{
				continue;
			}

			TArray<AParcelActor*>* CandidatesPtr = ByTag.Find(TagTarget);
			const int32 TotalUnits = CandidatesPtr ? GetCandidateUnits(*CandidatesPtr) : 0;
			const int32 CandidateCount = CandidatesPtr ? CandidatesPtr->Num() : 0;
			if (CandidatesPtr && TotalUnits >= MinUnits && CandidateCount >= MinContentCount)
			{
				MatchedTagsByRecipe.FindOrAdd(RecipeIndex).Add(TagTarget);
				EligibleTagRecipes.Add(RecipeIndex);
			}
		}
	}

	TArray<int32> EligibleRecipes;
	if (EligibleRowRecipes.Num() > 0)
	{
		EligibleRecipes = EligibleRowRecipes.Array();
	}
	else
	{
		EligibleRecipes = EligibleTagRecipes.Array();
	}

	if (EligibleRecipes.Num() == 0)
	{
		return;
	}

	const int32 SelectedIndex = EligibleRecipes[FMath::RandRange(0, EligibleRecipes.Num() - 1)];
	const FParcelPackageRecipe& Recipe = ParcelDataAsset->PackageRecipes[SelectedIndex];
	const bool bUseRowFilter = MatchedRowsByRecipe.Contains(SelectedIndex);

	FName MatchedRowName = NAME_None;
	FGameplayTag MatchedTag;
	if (bUseRowFilter)
	{
		const TArray<FName>& RowMatches = MatchedRowsByRecipe.FindChecked(SelectedIndex);
		MatchedRowName = RowMatches[FMath::RandRange(0, RowMatches.Num() - 1)];
	}
	else
	{
		const TArray<FGameplayTag>& TagMatches = MatchedTagsByRecipe.FindChecked(SelectedIndex);
		MatchedTag = TagMatches[FMath::RandRange(0, TagMatches.Num() - 1)];
	}

	TArray<AParcelActor*>* CandidatesPtr = bUseRowFilter
		? ByRowName.Find(MatchedRowName)
		: ByTag.Find(MatchedTag);
	if (!CandidatesPtr)
	{
		return;
	}

	// 소비할 파슬 선택: Min~Max 단위 범위 내, 최소 MinContentCount개 이상 포함 (가능하면 최대 수량 사용)
	const int32 MinUnits = Recipe.MinRequiredCount;
	const int32 MaxUnits = Recipe.MaxRequiredCount;
	const int32 MinContentCount = FMath::Max(1, Recipe.MinContentCount);
	const int32 MaxCount = CandidatesPtr->Num();

	TArray<AParcelActor*> Consume;
	// 2D DP: (sum, count) -> backtrack info. count = 물건 개수(내용물 개수)
	struct FPackState { int32 PrevSum = -1; int32 PrevCount = -1; int32 CandidateIndex = -1; };
	TArray<TArray<FPackState>> Dp;
	Dp.SetNum(MaxUnits + 1);
	for (int32 s = 0; s <= MaxUnits; ++s)
	{
		Dp[s].SetNum(MaxCount + 1);
	}
	Dp[0][0].PrevSum = -2; // 시작 상태 표시
	Dp[0][0].PrevCount = -2;

	for (int32 CandidateIndex = 0; CandidateIndex < MaxCount; ++CandidateIndex)
	{
		AParcelActor* Candidate = (*CandidatesPtr)[CandidateIndex];
		if (!Candidate)
		{
			continue;
		}

		const int32 CandidateUnits = FMath::Max(1, Candidate->GetPackagingSpaceUnits());
		for (int32 Sum = MaxUnits - CandidateUnits; Sum >= 0; --Sum)
		{
			for (int32 Count = MaxCount - 1; Count >= 0; --Count)
			{
				const bool bPrevValid = (Sum == 0 && Count == 0)
					? (Dp[0][0].PrevSum == -2)
					: (Dp[Sum][Count].PrevSum != -1);
				if (!bPrevValid)
				{
					continue;
				}
				const int32 NewSum = Sum + CandidateUnits;
				const int32 NewCount = Count + 1;
				if (NewSum <= MaxUnits && NewCount <= MaxCount && Dp[NewSum][NewCount].PrevSum == -1)
				{
					Dp[NewSum][NewCount].PrevSum = Sum;
					Dp[NewSum][NewCount].PrevCount = Count;
					Dp[NewSum][NewCount].CandidateIndex = CandidateIndex;
				}
			}
		}
	}

	// Min~Max 범위, MinContentCount개 이상인 조합 중 최대 수량 선택
	int32 TargetSum = INDEX_NONE;
	int32 TargetCount = INDEX_NONE;
	for (int32 S = MaxUnits; S >= MinUnits; --S)
	{
		for (int32 C = MaxCount; C >= MinContentCount; --C)
		{
			if (Dp[S][C].PrevSum != -1 || (S == 0 && C == 0 && Dp[0][0].PrevSum == -2))
			{
				TargetSum = S;
				TargetCount = C;
				break;
			}
		}
		if (TargetSum != INDEX_NONE)
		{
			break;
		}
	}

	if (TargetSum == INDEX_NONE || TargetCount == INDEX_NONE)
	{
		return; // Min~Max 범위, MinContentCount 이상 조합을 못 찾았으므로 포장 중단
	}

	// 백트래킹으로 Consume 구성
	int32 CurSum = TargetSum;
	int32 CurCount = TargetCount;
	while (CurSum > 0 || CurCount > 0)
	{
		FPackState& State = Dp[CurSum][CurCount];
		if (State.PrevSum == -2)
		{
			break;
		}
		if (State.CandidateIndex >= 0 && CandidatesPtr->IsValidIndex(State.CandidateIndex))
		{
			if (AParcelActor* P = (*CandidatesPtr)[State.CandidateIndex])
			{
				Consume.Add(P);
			}
		}
		const int32 NextSum = State.PrevSum;
		const int32 NextCount = State.PrevCount;
		if (NextSum < 0 && NextCount < 0)
		{
			break;
		}
		CurSum = NextSum;
		CurCount = NextCount;
	}

	if (Consume.Num() == 0)
	{
		return;
	}

	// 크레딧 소모 확인 (부족 시 포장 중단)
	const int32 RecipeCreditCost = Recipe.CreditCost;
	if (RecipeCreditCost > 0)
	{
		if (APickpackerGameMode* GameMode = GetWorld() ? Cast<APickpackerGameMode>(GetWorld()->GetAuthGameMode()) : nullptr)
		{
			if (APickpackerGameState* GameState = GameMode->GetGameState<APickpackerGameState>())
			{
				if (GameState->GetTeamCredits() < RecipeCreditCost)
				{
					if (bEnableDebugLogging)
					{
						UE_LOG(LogTemp, Warning, TEXT("[PackagingStationActor] Insufficient credits for packaging (need %d)"), RecipeCreditCost);
					}
					return;
				}
			}
		}
	}

	// 출력 위치
	FTransform OutTransform = OutputLocation ? OutputLocation->GetComponentTransform() : GetActorTransform();

	// 포장 액터 스폰 (PackedParcelActor 사용)
	TSubclassOf<AParcelActor> UseClass = Recipe.PackagedClass
		? Recipe.PackagedClass
		: (DefaultPackagedClass ? DefaultPackagedClass : TSubclassOf<AParcelActor>(APackedParcelActor::StaticClass()));
	if (!UseClass)
	{
		return;
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
				ContentRow = MatchedRowName;
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
			// 레시피 인덱스 기반 설정 (가장 안전한 식별자)
			PackedParcel->SetPackageRecipeIndex(SelectedIndex);
			
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
		
		// 포장 시 크레딧 소모
		if (RecipeCreditCost > 0)
		{
			if (APickpackerGameMode* GameMode = GetWorld() ? Cast<APickpackerGameMode>(GetWorld()->GetAuthGameMode()) : nullptr)
			{
				GameMode->ApplyCreditDelta(-RecipeCreditCost, FString::Printf(TEXT("Packaging cost: %s"), *Recipe.RecipeName));
			}
		}

		// PackedParcelActor는 이미 포장 상태로 생성되므로 SetPackaged 불필요
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Log, TEXT("[PackagingStationActor] Packaged bundle spawned: %s (Row: %s / Tag: %s)"),
				*Packaged->GetName(),
				*MatchedRowName.ToString(),
				*MatchedTag.ToString());
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
