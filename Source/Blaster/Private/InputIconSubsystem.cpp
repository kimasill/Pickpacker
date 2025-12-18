#include "InputIconSubsystem.h"

#include "InputIconSettings.h"
#include "InputLibrary.h"
#include "Engine/DataTable.h"

void UInputIconSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	InitializeFromSettings();
}

void UInputIconSubsystem::Deinitialize()
{
	SetIconTable(nullptr);
	Super::Deinitialize();
}

void UInputIconSubsystem::SetIconTable(UDataTable* Table)
{
	IconTable = Table;
	UInputLibrary::SetIconDataTable(IconTable);
}

void UInputIconSubsystem::InitializeFromSettings()
{
	const UInputIconSettings* Settings = GetDefault<UInputIconSettings>();
	if (!Settings)
	{
		UE_LOG(LogTemp, Warning, TEXT("[InputIconSubsystem] Unable to load InputIconSettings; key icons will be unavailable."));
		return;
	}

	if (Settings->IconDataTable.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[InputIconSubsystem] Icon data table not assigned in settings."));
		return;
	}

	UDataTable* LoadedTable = Settings->IconDataTable.LoadSynchronous();
	if (!LoadedTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("[InputIconSubsystem] Failed to load icon data table asset %s"), *Settings->IconDataTable.ToString());
		return;
	}

	SetIconTable(LoadedTable);
}
