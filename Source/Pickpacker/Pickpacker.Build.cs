// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Pickpacker : ModuleRules
{
	public Pickpacker(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivatePCHHeaderFile = "Pickpacker.h";
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "Slate", "SlateCore", "Niagara", "MultiplayerSessions", "OnlineSubsystem", "OnlineSubsystemUtils", "GameplayTags", "AIModule", "NavigationSystem", "DeveloperSettings", "LevelSequence", "MovieScene", "Sockets" });
        DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");


        // Editor-only dependencies should only be added when building the editor
        if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"Blutility",
				"UnrealEd",
				"UMGEditor",
                "GameplayTags"
            });
		}

		// Add module include paths for headers organized by folders (no Public/Private split)
		PublicIncludePaths.AddRange(new string[]
		{
			Path.Combine(ModuleDirectory),
			Path.Combine(ModuleDirectory, "GameMode"),
			Path.Combine(ModuleDirectory, "GameState"),
			Path.Combine(ModuleDirectory, "Subsystem"),
			Path.Combine(ModuleDirectory, "PCG"),
			Path.Combine(ModuleDirectory, "Components"),
			Path.Combine(ModuleDirectory, "PickpackerTypes"),
			Path.Combine(ModuleDirectory, "Interaction"),
			Path.Combine(ModuleDirectory, "Interfaces"),
			Path.Combine(ModuleDirectory, "NPC"),
			Path.Combine(ModuleDirectory, "DataAssets")
		});
	}
}
