// Copyright (c) 2025-2026 Winyunq. All rights reserved.
// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UmgMcp : ModuleRules
{
	public UmgMcp(ReadOnlyTargetRules Target) : base(Target)
	{
		bUseUnity = false;
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDefinitions.Add("UMGMCP_EXPORTS=1");

		PublicIncludePaths.AddRange(
			new string[] {
				ModuleDirectory + "/Public",
				ModuleDirectory + "/Public/FabServer"
			}
		);
		
		PrivateIncludePaths.AddRange(
			new string[] {
				ModuleDirectory + "/Private",
				ModuleDirectory + "/Private/FabServer/ChatUI",
				ModuleDirectory + "/Private/FabServer/ChatUI/MessageInteractionHub",
			}
		);
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Networking",
				"Sockets",
				"HTTP",
				"Json",
				"JsonUtilities",
				"DeveloperSettings",
				"PhysicsCore",
				"UMG",
				"MovieScene",
				"MovieSceneTracks",
				"MaterialEditor",
                "Slate",
				"SlateCore",
				"LiteRTLMUnreal"
			}
		);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"ApplicationCore", // For ClipboardCopy
				"EditorScriptingUtilities",
				"EditorSubsystem",
				"Kismet",
				"KismetCompiler",
				"BlueprintGraph",
				"Projects",
				"AssetRegistry",
				"Settings",
				"WorkspaceMenuStructure",
				"MaterialEditor",
				"ImageWrapper",
				"Serialization"

			}
		);
		
		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
					"UMGEditor",
					"PropertyEditor",
					"ToolMenus",
					"BlueprintEditorLibrary"
				}
			);
		}
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
		);
	}
}
