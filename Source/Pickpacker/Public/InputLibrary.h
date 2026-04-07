// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InputLibrary.generated.h"

USTRUCT(BlueprintType)
struct FInputIconRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Helper")
	UTexture2D* Icon = nullptr;
};

UCLASS()
class PICKPACKER_API UInputLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UI|Helper")
	static void SetIconDataTable(UDataTable* Table);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI|Helper")
	static UDataTable* GetIconDataTable();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI|Helper")
	static UTexture2D* GetIconForKeyFromTable(UDataTable* Table, const FKey& Key);

	UFUNCTION(BlueprintCallable, Category = "UI|Helper")
	static UTexture2D* GetIconForKey(const FKey& Key);
};
