// Fill out your copyright notice in the Description page of Project Settings.

#include "PackedParcelActor.h"
#include "Blaster/DataAssets/DA_ParcelData.h"
#include "Blaster/Parcel/ParcelActor.h"

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
	if (!ParcelDataAsset || PackageRecipeRowName == NAME_None)
	{
		return;
	}

	// PackageRecipe 찾기
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
		const int32 MeshIndex = FMath::RandRange(0, FoundRecipe->PackagedMeshes.Num() - 1);
		PackageMeshAsset = FoundRecipe->PackagedMeshes[MeshIndex];
		UpdateMeshForCurrentPackagingState();
	}

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[PackedParcelActor] Initialized from PackageRecipe: %s"), *PackageRecipeRowName.ToString());
	}
}

void APackedParcelActor::SetPackageRecipeRowName(const FName& InRowName)
{
	PackageRecipeRowName = InRowName;
	InitializeFromPackageRecipe();
}

void APackedParcelActor::SetInitialContents(const TArray<FParcelPackageContent>& InContents)
{
	InitialContents = InContents;
	SetPackageContents(InContents);
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

