using UnrealBuildTool;

public class LiteRTDemo : ModuleRules
{
	public LiteRTDemo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "LiteRTLMUnreal", "UMG", "Slate", "SlateCore" });

		if (Target.bBuildEditor)
		{
			PublicDependencyModuleNames.Add("UmgMcp");
		}

		PrivateDependencyModuleNames.AddRange(new string[] {  });
	}
}
