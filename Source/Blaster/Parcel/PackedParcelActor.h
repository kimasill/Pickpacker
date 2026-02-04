// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blaster/Parcel/ParcelActor.h"
#include "PackedParcelActor.generated.h"

class AParcelSpawnMarker;

/**
 * Packed Parcel Actor - 포장 전용
 * PackageRecipe의 RowName을 사용하여 설정됨
 * 초기 콘텐츠 지정 가능
 */
UCLASS(BlueprintType, Blueprintable)
class BLASTER_API APackedParcelActor : public AParcelActor
{
	GENERATED_BODY()

public:
	APackedParcelActor();

	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/**
	 * PackageRecipe RowName을 사용하여 포장 레시피 적용
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel|Config")
	void SetPackageRecipeRowName(const FName& InRowName);

	/**
	 * PackageRecipe 인덱스를 사용하여 포장 레시피 적용
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel|Config")
	void SetPackageRecipeIndex(int32 InIndex);

	/**
	 * 초기 콘텐츠 설정 (에디터에서 지정 가능)
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel|Config")
	void SetInitialContents(const TArray<FParcelPackageContent>& InContents);

protected:
	/**
	 * PackageRecipe RowName 기반 초기화
	 */
	void InitializeFromPackageRecipe();
	void InitializeRandomContentsFromSpawnMarker(const FParcelPackageRecipe& Recipe);

	/** PackageRecipe RowName (포장 레시피 선택용) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config", meta = (GetOptions = "GetPackageRecipeRowOptions"))
	FName PackageRecipeRowName = NAME_None;

	/** PackageRecipe 인덱스 (RowName보다 우선) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config")
	int32 PackageRecipeIndex = INDEX_NONE;

	/** 초기 콘텐츠 (에디터에서 지정 가능, 런타임에는 PackageContents로 사용됨) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config|Contents")
	TArray<FParcelPackageContent> InitialContents;

	/** 랜덤 콘텐츠 생성에 사용할 스폰 마커 (직접 지정) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config|Contents")
	TObjectPtr<AParcelSpawnMarker> ContentSpawnMarker = nullptr;

	/** 콘텐츠에 사용할 Parcel 클래스 (일괄 적용) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config|Contents")
	TSubclassOf<AParcelActor> ContentParcelClass;

	void ApplyContentParcelClass(TArray<FParcelPackageContent>& Contents) const;

public:
	/**
	 * PackageRecipe RowName 옵션 제공
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel|Config")
	TArray<FName> GetPackageRecipeRowOptions() const;
};

