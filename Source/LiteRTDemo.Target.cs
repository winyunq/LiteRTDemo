using UnrealBuildTool;
using System.Collections.Generic;

public class LiteRTDemoTarget : TargetRules
{
	public LiteRTDemoTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("LiteRTDemo");
	}
}
