#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "InputIconSubsystem.generated.h"

class UDataTable;

/**
 * Central subsystem that owns the input icon data table and shares it with UI widgets
 */
UCLASS()
class PICKPACKER_API UInputIconSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Manually override the icon table at runtime */
	UFUNCTION(BlueprintCallable, Category = "Input Icons")
	void SetIconTable(UDataTable* Table);

	/** Access the icon table currently registered */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Input Icons")
	UDataTable* GetIconTable() const { return IconTable; }

private:
	void InitializeFromSettings();

	/** Cached icon table shared with widgets */
	UPROPERTY(Transient)
	UDataTable* IconTable = nullptr;
};
