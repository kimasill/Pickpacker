// Copyright Epic Games, Inc. All Rights Reserved.

#include "Pickpacker.h"

#define LOCTEXT_NAMESPACE "FPickpackerModule"

void FPickpackerModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FPickpackerModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FPickpackerModule, Pickpacker)