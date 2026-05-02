using UnrealBuildTool;
using System.Collections.Generic;

public class LiteRTDemoTarget : TargetRules
{
	public LiteRTDemoTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("LiteRTDemo");
	}
}
