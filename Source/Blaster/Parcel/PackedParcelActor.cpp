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
	for (const FParcelPackageRecipe& Recipe : ParcelDataAsset->PackageRecipes)
	{
		if (Recipe.TargetParcelRowName == PackageRecipeRowName || 
			(Recipe.TargetParcelRowName == NAME_None && Recipe.TargetParcelTag.GetTagName() == PackageRecipeRowName))
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

	// 포장 상태 강제 설정 (복제 기본값 상쇄)
	SetPackaged(true);

	// 포장 메시 설정
	if (!FoundRecipe->PackagedMesh.IsNull())
	{
		PackageMeshAsset = FoundRecipe->PackagedMesh;
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
		FString Label = Recipe.RecipeName != TEXT("Default Package Recipe")
			? Recipe.RecipeName
			: FString::Printf(TEXT("Recipe_%d"), Index);
		Options.Add(FName(*Label));
	}

	return Options;
}

