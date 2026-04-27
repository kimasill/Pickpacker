// Copyright Epic Games, Inc. All Rights Reserved.

#include "Pickpacker.h"
#include "Modules/ModuleManager.h"

class FPickpackerModule final : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();

		FModuleManager::Get().LoadModule(TEXT("BlueprintFileUtils"));
		FModuleManager::Get().LoadModule(TEXT("JsonBlueprintUtilities"));

#if WITH_EDITOR
		FModuleManager::Get().LoadModule(TEXT("JsonBlueprintGraph"));
#endif
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FPickpackerModule, Pickpacker, "Pickpacker");
