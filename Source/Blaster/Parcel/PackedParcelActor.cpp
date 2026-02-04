// Fill out your copyright notice in the Description page of Project Settings.

#include "PackedParcelActor.h"
#include "Blaster/DataAssets/DA_ParcelData.h"
#include "Blaster/Parcel/ParcelActor.h"
#include "Spawning/ParcelSpawnMarker.h"

APackedParcelActor::APackedParcelActor()
{
	// PackedParcelActor는 항상 포장 상태
	bIsPackaged = true;
	bIsItem = false;
	bIsPackageBundle = true;
}

void APackedParcelActor::BeginPlay()
{
	// PackageRecipe 기반 초기화
	InitializeFromPackageRecipe();

	// 초기 콘텐츠가 지정되어 있으면 PackageContents로 설정
	// (InitialContents는 에디터용, PackageContents는 런타임용)
	if (InitialContents.Num() > 0)
	{
		SetPackageContents(InitialContents);
	}

	// 부모 클래스 BeginPlay 호출
	Super::BeginPlay();

	// Base BeginPlay에서 ItemData가 갱신되므로 레시피 GripType을 다시 반영
	if (HasAuthority() && ParcelDataAsset && PackageRecipeRowName != NAME_None)
	{
		const FParcelPackageRecipe* FoundRecipe = nullptr;
		const FString RequestedRecipeName = PackageRecipeRowName.ToString();
		for (const FParcelPackageRecipe& Recipe : ParcelDataAsset->PackageRecipes)
		{
			bool bMatchesTargetRowList = false;
			for (const FName& RowName : Recipe.TargetParcelRowNames)
			{
				if (RowName == PackageRecipeRowName)
				{
					bMatchesTargetRowList = true;
					break;
				}
			}

			bool bMatchesTargetTagList = false;
			for (const FGameplayTag& Tag : Recipe.TargetParcelTags)
			{
				if (Tag.GetTagName() == PackageRecipeRowName)
				{
					bMatchesTargetTagList = true;
					break;
				}
			}

			const bool bMatchesRecipeName =
				!RequestedRecipeName.IsEmpty() &&
				Recipe.RecipeName.Equals(RequestedRecipeName, ESearchCase::IgnoreCase);
			if (bMatchesRecipeName || bMatchesTargetRowList || bMatchesTargetTagList)
			{
				FoundRecipe = &Recipe;
				break;
			}
		}

		if (FoundRecipe && FoundRecipe->GripType != EGripType::None)
		{
			FItemData UpdatedItemData = ItemData;
			UpdatedItemData.GripType = FoundRecipe->GripType;
			SetItemData(UpdatedItemData);
		}
	}
}

void APackedParcelActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 에디터에서도 초기화
	if (!HasActorBegunPlay())
	{
		InitializeFromPackageRecipe();
	}
}

void APackedParcelActor::InitializeFromPackageRecipe()
{
	if (!ParcelDataAsset)
	{
		return;
	}

	// PackageRecipe 찾기
	const FParcelPackageRecipe* FoundRecipe = nullptr;
	if (PackageRecipeIndex != INDEX_NONE && ParcelDataAsset->PackageRecipes.IsValidIndex(PackageRecipeIndex))
	{
		FoundRecipe = &ParcelDataAsset->PackageRecipes[PackageRecipeIndex];
	}
	if (!FoundRecipe && PackageRecipeRowName != NAME_None)
	{
		const FString RequestedRecipeName = PackageRecipeRowName.ToString();
		for (const FParcelPackageRecipe& Recipe : ParcelDataAsset->PackageRecipes)
		{
			bool bMatchesTargetRowList = false;
			for (const FName& RowName : Recipe.TargetParcelRowNames)
			{
				if (RowName == PackageRecipeRowName)
				{
					bMatchesTargetRowList = true;
					break;
				}
			}

			bool bMatchesTargetTagList = false;
			for (const FGameplayTag& Tag : Recipe.TargetParcelTags)
			{
				if (Tag.GetTagName() == PackageRecipeRowName)
				{
					bMatchesTargetTagList = true;
					break;
				}
			}

			const bool bMatchesRecipeName =
				!RequestedRecipeName.IsEmpty() &&
				Recipe.RecipeName.Equals(RequestedRecipeName, ESearchCase::IgnoreCase);
			if (bMatchesRecipeName || bMatchesTargetRowList || bMatchesTargetTagList)
			{
				FoundRecipe = &Recipe;
				break;
			}
		}
	}

	if (!FoundRecipe)
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PackedParcelActor] Failed to find PackageRecipe for RowName: %s"), *PackageRecipeRowName.ToString());
		}
		return;
	}

	// PackageRecipe 적용
	SetPackageRecipe(FoundRecipe, ParcelDataAsset);

	// 콘텐츠 자동 생성 (초기 콘텐츠가 비어 있을 때만)
	InitializeRandomContentsFromSpawnMarker(*FoundRecipe);

	// ParcelDefinition은 레시피의 RowName 목록 중 매칭된 값으로 동기화
	if (FoundRecipe->TargetParcelRowNames.Contains(PackageRecipeRowName) && ParcelDefinitionRowName != PackageRecipeRowName)
	{
		ParcelDefinitionRowName = PackageRecipeRowName;
		ApplyParcelConfigFromDataAsset(false);
	}

	// 포장 상태 강제 설정 (복제 기본값 상쇄)
	SetPackaged(true);

	// 포장 메시 설정
	if (FoundRecipe->PackagedMeshes.Num() > 0)
	{
		const int32 ExistingIndex = PackageMeshAsset.IsNull()
			? INDEX_NONE
			: FoundRecipe->PackagedMeshes.IndexOfByKey(PackageMeshAsset);
		if (ExistingIndex == INDEX_NONE)
		{
			const int32 MeshIndex = FMath::RandRange(0, FoundRecipe->PackagedMeshes.Num() - 1);
			PackageMeshAsset = FoundRecipe->PackagedMeshes[MeshIndex];
		}
		UpdateMeshForCurrentPackagingState();
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PackedParcelActor] Initialized from PackageRecipe: %s"), *PackageRecipeRowName.ToString());
	}
}

void APackedParcelActor::InitializeRandomContentsFromSpawnMarker(const FParcelPackageRecipe& Recipe)
{
	if (InitialContents.Num() > 0)
	{
		return;
	}

	if (PackageRequiredCount <= 0)
	{
		return;
	}

	const AParcelSpawnMarker* SelectedMarker = ContentSpawnMarker;
	if (!SelectedMarker)
	{
		return;
	}
	if (SelectedMarker->CandidateParcelRows.Num() == 0)
	{
		return;
	}

	UDA_ParcelData* DataAssetToUse = ParcelDataAsset ? ParcelDataAsset : SelectedMarker->ParcelDataAsset;
	if (!DataAssetToUse)
	{
		return;
	}

	struct FWeightedRow
	{
		FName RowName = NAME_None;
		float Weight = 0.0f;
		int32 Units = 1;
		FGameplayTag ParcelTag;
	};

	TArray<FWeightedRow> Candidates;
	Candidates.Reserve(SelectedMarker->CandidateParcelRows.Num());
	const bool bHasTagFilter = Recipe.TargetParcelTags.Num() > 0;
	for (const FParcelSpawnCandidate& Candidate : SelectedMarker->CandidateParcelRows)
	{
		if (Candidate.ParcelRow == NAME_None)
		{
			continue;
		}

		FParcelConfig Config;
		if (!DataAssetToUse->GetParcelConfigByName(Candidate.ParcelRow, Config))
		{
			continue;
		}

		if (!Config.ParcelTag.IsValid())
		{
			continue;
		}
		if (bHasTagFilter && !Recipe.TargetParcelTags.Contains(Config.ParcelTag))
		{
			continue;
		}

		FWeightedRow Entry;
		Entry.RowName = Candidate.ParcelRow;
		Entry.Weight = Candidate.Weight;
		Entry.Units = FMath::Max(1, Config.PackagingSpaceUnits);
		Entry.ParcelTag = Config.ParcelTag;
		Candidates.Add(Entry);
	}

	if (Candidates.Num() == 0)
	{
		return;
	}

	FGameplayTag SelectedTag;
	if (bHasTagFilter)
	{
		TMap<FGameplayTag, float> TagWeights;
		for (const FWeightedRow& Candidate : Candidates)
		{
			TagWeights.FindOrAdd(Candidate.ParcelTag) += FMath::Max(0.0f, Candidate.Weight);
		}

		float TotalTagWeight = 0.0f;
		for (const TPair<FGameplayTag, float>& Pair : TagWeights)
		{
			TotalTagWeight += Pair.Value;
		}

		if (TotalTagWeight > 0.0f)
		{
			const float Pick = FMath::FRandRange(0.0f, TotalTagWeight);
			float Accumulated = 0.0f;
			for (const TPair<FGameplayTag, float>& Pair : TagWeights)
			{
				Accumulated += Pair.Value;
				if (Pick <= Accumulated)
				{
					SelectedTag = Pair.Key;
					break;
				}
			}
		}

		if (!SelectedTag.IsValid())
		{
			TArray<FGameplayTag> ValidTags;
			TagWeights.GetKeys(ValidTags);
			if (ValidTags.Num() == 0)
			{
				return;
			}
			const int32 Picked = FMath::RandRange(0, ValidTags.Num() - 1);
			SelectedTag = ValidTags[Picked];
		}
	}
	else
	{
		const int32 Picked = FMath::RandRange(0, Candidates.Num() - 1);
		SelectedTag = Candidates[Picked].ParcelTag;
	}

	TMap<FName, int32> ContentCounts;
	int32 RemainingUnits = PackageRequiredCount;
	int32 SafetyCounter = 200;
	while (RemainingUnits > 0 && SafetyCounter-- > 0)
	{
		float TotalWeight = 0.0f;
		int32 FitCount = 0;
		for (const FWeightedRow& Candidate : Candidates)
		{
			if (Candidate.ParcelTag == SelectedTag && Candidate.Units <= RemainingUnits)
			{
				TotalWeight += FMath::Max(0.0f, Candidate.Weight);
				++FitCount;
			}
		}

		if (FitCount == 0)
		{
			break;
		}

		int32 ChosenIndex = INDEX_NONE;
		if (TotalWeight > 0.0f)
		{
			const float Pick = FMath::FRandRange(0.0f, TotalWeight);
			float Accumulated = 0.0f;
			for (int32 Index = 0; Index < Candidates.Num(); ++Index)
			{
				const FWeightedRow& Candidate = Candidates[Index];
				if (Candidate.ParcelTag != SelectedTag || Candidate.Units > RemainingUnits || Candidate.Weight <= 0.0f)
				{
					continue;
				}
				Accumulated += Candidate.Weight;
				if (Pick <= Accumulated)
				{
					ChosenIndex = Index;
					break;
				}
			}
		}

		if (ChosenIndex == INDEX_NONE)
		{
			TArray<int32> FitIndices;
			FitIndices.Reserve(FitCount);
			for (int32 Index = 0; Index < Candidates.Num(); ++Index)
			{
				if (Candidates[Index].ParcelTag == SelectedTag && Candidates[Index].Units <= RemainingUnits)
				{
					FitIndices.Add(Index);
				}
			}

			if (FitIndices.Num() == 0)
			{
				break;
			}

			const int32 Picked = FMath::RandRange(0, FitIndices.Num() - 1);
			ChosenIndex = FitIndices[Picked];
		}

		if (!Candidates.IsValidIndex(ChosenIndex))
		{
			break;
		}

		const FWeightedRow& PickedRow = Candidates[ChosenIndex];
		ContentCounts.FindOrAdd(PickedRow.RowName)++;
		RemainingUnits -= PickedRow.Units;
	}

	if (ContentCounts.Num() == 0)
	{
		return;
	}

	TArray<FParcelPackageContent> AutoContents;
	AutoContents.Reserve(ContentCounts.Num());
	for (const TPair<FName, int32>& Pair : ContentCounts)
	{
		FParcelPackageContent Content;
		Content.ParcelRowName = Pair.Key;
		Content.Count = Pair.Value;
		AutoContents.Add(Content);
	}

	ApplyContentParcelClass(AutoContents);
	InitialContents = AutoContents;
	SetPackageContents(AutoContents);
}

void APackedParcelActor::SetPackageRecipeRowName(const FName& InRowName)
{
	PackageRecipeRowName = InRowName;
	PackageRecipeIndex = INDEX_NONE;
	InitializeFromPackageRecipe();
}

void APackedParcelActor::SetPackageRecipeIndex(int32 InIndex)
{
	PackageRecipeIndex = InIndex;
	PackageRecipeRowName = NAME_None;
	InitializeFromPackageRecipe();
}

void APackedParcelActor::SetInitialContents(const TArray<FParcelPackageContent>& InContents)
{
	InitialContents = InContents;
	ApplyContentParcelClass(InitialContents);
	SetPackageContents(InitialContents);
}

void APackedParcelActor::ApplyContentParcelClass(TArray<FParcelPackageContent>& Contents) const
{
	if (!ContentParcelClass)
	{
		return;
	}

	for (FParcelPackageContent& Content : Contents)
	{
		Content.ParcelClass = ContentParcelClass;
	}
}

TArray<FName> APackedParcelActor::GetPackageRecipeRowOptions() const
{
	TArray<FName> Options;
	if (!ParcelDataAsset)
	{
		return Options;
	}

	const int32 Num = ParcelDataAsset->PackageRecipes.Num();
	Options.Reserve(Num);
	for (int32 Index = 0; Index < Num; ++Index)
	{
		const FParcelPackageRecipe& Recipe = ParcelDataAsset->PackageRecipes[Index];
		FString Label = Recipe.RecipeName;
		if (Label.IsEmpty())
		{
			if (Recipe.TargetParcelRowNames.Num() > 0)
			{
				Label = Recipe.TargetParcelRowNames[0].ToString();
			}
			else if (Recipe.TargetParcelTags.Num() > 0)
			{
				Label = Recipe.TargetParcelTags[0].ToString();
			}
		}
		if (Label.IsEmpty())
		{
			Label = FString::Printf(TEXT("Recipe_%d"), Index);
		}
		Options.Add(FName(*Label));
	}

	return Options;
}

