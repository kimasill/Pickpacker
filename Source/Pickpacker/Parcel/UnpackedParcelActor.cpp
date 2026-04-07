// Fill out your copyright notice in the Description page of Project Settings.

#include "UnpackedParcelActor.h"
#include "DataAssets/DA_ParcelData.h"
#include "Parcel/ParcelActor.h"

AUnpackedParcelActor::AUnpackedParcelActor()
{
	// UnpackedParcelActor는 항상 비포장 상태
	bIsPackaged = false;
	bIsItem = true;
	bIsPackageBundle = false;
}

void AUnpackedParcelActor::BeginPlay()
{
	// ParcelConfig 기반 초기화
	InitializeFromParcelConfig();

	// 부모 클래스 BeginPlay 호출
	Super::BeginPlay();
}

void AUnpackedParcelActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 에디터에서도 초기화
	if (!HasActorBegunPlay())
	{
		InitializeFromParcelConfig();
	}
}

void AUnpackedParcelActor::InitializeFromParcelConfig()
{
	if (!ParcelDataAsset || ParcelDefinitionRowName == NAME_None)
	{
		return;
	}

	FParcelConfig Config;
	if (!ParcelDataAsset->GetParcelConfigByName(ParcelDefinitionRowName, Config))
	{
		if (bEnableDebugLogging)
		{
			UE_LOG(LogTemp, Warning, TEXT("[UnpackedParcelActor] Failed to find ParcelConfig for RowName: %s"), *ParcelDefinitionRowName.ToString());
		}
		return;
	}

	// ParcelConfig 기반 초기화
	InitializeParcel(Config);
	
	// 메시 업데이트
	UpdateMeshForCurrentPackagingState();
}

void AUnpackedParcelActor::SetPackageRecipeRowName(const FName& InRowName)
{
	PackageRecipeRowName = InRowName;
	// UnpackedParcelActor는 PackageRecipe를 사용하지 않음
}

TArray<FName> AUnpackedParcelActor::GetPackageRecipeRowOptions() const
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
		FString Label;
		if (Recipe.TargetParcelRowNames.Num() > 0)
		{
			Label = Recipe.TargetParcelRowNames[0].ToString();
		}
		else if (Recipe.TargetParcelTags.Num() > 0)
		{
			Label = Recipe.TargetParcelTags[0].ToString();
		}
		if (Label.IsEmpty())
		{
			Label = FString::Printf(TEXT("Recipe_%d"), Index);
		}
		Options.Add(FName(*Label));
	}

	return Options;
}

