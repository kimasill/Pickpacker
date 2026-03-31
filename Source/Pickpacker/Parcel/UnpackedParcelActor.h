// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blaster/Parcel/ParcelActor.h"
#include "UnpackedParcelActor.generated.h"

/**
 * Unpacked Parcel Actor - 비포장 단일 오브젝트 전용
 * ParcelConfig의 RowName을 사용하여 설정됨
 */
UCLASS(BlueprintType, Blueprintable)
class BLASTER_API AUnpackedParcelActor : public AParcelActor
{
	GENERATED_BODY()

public:
	AUnpackedParcelActor();

	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/**
	 * PackageRecipe RowName을 사용하여 포장 레시피 적용
	 * (UnpackedParcelActor는 사용하지 않음)
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel|Config")
	void SetPackageRecipeRowName(const FName& InRowName);

protected:
	/**
	 * ParcelConfig RowName 기반 초기화
	 */
	void InitializeFromParcelConfig();

	/** PackageRecipe RowName (포장 레시피 선택용) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parcel Config", meta = (GetOptions = "GetPackageRecipeRowOptions"))
	FName PackageRecipeRowName = NAME_None;

public:
	/**
	 * PackageRecipe RowName 옵션 제공
	 */
	UFUNCTION(BlueprintCallable, Category = "Parcel|Config")
	TArray<FName> GetPackageRecipeRowOptions() const;
};

