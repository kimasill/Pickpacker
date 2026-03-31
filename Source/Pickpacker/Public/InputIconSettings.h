#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/DeveloperSettings.h"
#include "InputIconSettings.generated.h"

/**
 * Editor-configurable settings for input icon data
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Input Icon Settings"))
class BLASTER_API UInputIconSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UInputIconSettings();
		
	/** Data table that maps input keys to icon textures */
	UPROPERTY(EditAnywhere, Config, Category = "Input Icons")
	TSoftObjectPtr<UDataTable> IconDataTable;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	virtual FName GetSectionName() const override { return TEXT("InputIcons"); }
};
