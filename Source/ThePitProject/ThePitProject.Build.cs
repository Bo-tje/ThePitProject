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
			"OSC",
			"MetasoundEngine",
			"AudioExtensions",
			"AudioMixer",
			"Synthesis"
		});

		PublicIncludePaths.AddRange(new string[] {
			ModuleDirectory,
			System.IO.Path.Combine(ModuleDirectory, "Core"),
			System.IO.Path.Combine(ModuleDirectory, "Subsystems"),
			System.IO.Path.Combine(ModuleDirectory, "Concepts/AirHockey"),
			System.IO.Path.Combine(ModuleDirectory, "Concepts/HarmonicConvergence"),
			System.IO.Path.Combine(ModuleDirectory, "Concepts/RhythmPressure")
		});
	}
}
