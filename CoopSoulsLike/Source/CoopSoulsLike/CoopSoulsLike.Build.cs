// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CoopSoulsLike : ModuleRules
{
	public CoopSoulsLike(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
            "OnlineSubsystem",
            "OnlineSubsystemUtils",
			"NetCore"
        });

        DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");

        PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"CoopSoulsLike",
			"CoopSoulsLike/Variant_Platforming",
			"CoopSoulsLike/Variant_Platforming/Animation",
			"CoopSoulsLike/Variant_Combat",
			"CoopSoulsLike/Variant_Combat/AI",
			"CoopSoulsLike/Variant_Combat/Animation",
			"CoopSoulsLike/Variant_Combat/Gameplay",
			"CoopSoulsLike/Variant_Combat/Interfaces",
			"CoopSoulsLike/Variant_Combat/UI",
			"CoopSoulsLike/Variant_SideScrolling",
			"CoopSoulsLike/Variant_SideScrolling/AI",
			"CoopSoulsLike/Variant_SideScrolling/Gameplay",
			"CoopSoulsLike/Variant_SideScrolling/Interfaces",
			"CoopSoulsLike/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
