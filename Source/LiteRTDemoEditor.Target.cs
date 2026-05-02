using UnrealBuildTool;
using System.Collections.Generic;

public class LiteRTDemoEditorTarget : TargetRules
{
	public LiteRTDemoEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("LiteRTDemo");
	}
}
