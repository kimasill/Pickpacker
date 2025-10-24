// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Blaster : ModuleRules
{
	public Blaster(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "Niagara", "MultiplayerSessions", "OnlineSubsystem", "OnlineSubsystemSteam", "PCG" });

		PrivateDependencyModuleNames.AddRange(new string[] {});

		// Add module include paths for headers organized by folders (no Public/Private split)
		PublicIncludePaths.AddRange(new string[]
		{
			Path.Combine(ModuleDirectory),
			Path.Combine(ModuleDirectory, "GameMode"),
			Path.Combine(ModuleDirectory, "GameState"),
			Path.Combine(ModuleDirectory, "Subsystem"),
			Path.Combine(ModuleDirectory, "PCG"),
			Path.Combine(ModuleDirectory, "Components"),
			Path.Combine(ModuleDirectory, "PickpackerTypes")
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
