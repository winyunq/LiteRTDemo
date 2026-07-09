using UnrealBuildTool;

public class LiteRTDemo : ModuleRules
{
	public LiteRTDemo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Json", "LiteRTLMUnreal", "UMG", "Slate", "SlateCore" });

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PrivateDependencyModuleNames.Add("Launch");
		}

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemLibraries.Add("Comdlg32.lib");
		}

		if (Target.bBuildEditor)
		{
			PublicDependencyModuleNames.Add("UmgMcp");
		}

		PrivateDependencyModuleNames.AddRange(new string[] {  });
	}
}
