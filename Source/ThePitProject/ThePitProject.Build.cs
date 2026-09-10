// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ThePitProject : ModuleRules
{
	public ThePitProject(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { 
			"Core", 
			"CoreUObject", 
			"Engine", 
			"InputCore", 
			"EnhancedInput",
			"OSC"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {  });
	}
}
