using UnrealBuildTool;

public class LiteRTDemo : ModuleRules
{
	public LiteRTDemo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "LiteRTLMUnreal", "UMG", "Slate", "SlateCore", "UmgMcp" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });
	}
}
