#include "InputLibrary.h"

#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "UObject/UnrealType.h"

namespace
{
	TWeakObjectPtr<UDataTable> GIconDataTable;
}

void UInputLibrary::SetIconDataTable(UDataTable* Table)
{
	GIconDataTable = Table;
}

UDataTable* UInputLibrary::GetIconDataTable()
{
	return GIconDataTable.Get();
}

UTexture2D* UInputLibrary::GetIconForKeyFromTable(UDataTable* Table, const FKey& Key)
{
	if (!Table)
	{
		return nullptr;
	}

	const UScriptStruct* RowStruct = Table->GetRowStruct();
	const FName RowName = Key.GetFName();

	// Primary path: matching expected struct
	if (RowStruct == FInputIconRow::StaticStruct())
	{
		const FInputIconRow* IconRow = Table->FindRow<FInputIconRow>(RowName, TEXT("GetIconForKeyFromTable"));
		if (!IconRow)
		{
			return nullptr;
		}

		return IconRow->Icon;
	}

	// Fallback path: generic struct (e.g., S_InputPromptsInfos). Try to locate first UTexture2D property.
	void* RowData = Table->FindRowUnchecked(RowName);
	if (!RowData)
	{
		return nullptr;
	}

	for (TFieldIterator<FObjectProperty> It(RowStruct); It; ++It)
	{
		const FObjectProperty* ObjProp = *It;
		if (ObjProp && ObjProp->PropertyClass && ObjProp->PropertyClass->IsChildOf(UTexture2D::StaticClass()))
		{
			if (UTexture2D* IconTex = Cast<UTexture2D>(ObjProp->GetObjectPropertyValue_InContainer(RowData)))
			{
				return IconTex;
			}
		}
	}

	return nullptr;
}

UTexture2D* UInputLibrary::GetIconForKey(const FKey& Key)
{
	return GetIconForKeyFromTable(GetIconDataTable(), Key);
}
