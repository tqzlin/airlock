// Copyright Airlock project. All Rights Reserved.

using UnrealBuildTool;

public class Airlock : ModuleRules
{
	public Airlock(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Module root is an include path so headers can be referenced as "Core/AirlockGameMode.h" etc.
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"NetCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
