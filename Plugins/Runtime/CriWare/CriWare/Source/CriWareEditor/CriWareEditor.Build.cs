/****************************************************************************
 *
 * CRI Middleware SDK
 *
 * Copyright (c) 2013-2015 CRI Middleware Co., Ltd.
 *
 * Library  : CRIWARE plugin for Unreal Engine 4
 * Module   : Build Settings of CriWareEditor Module
 * File     : CriWareEditor.Build.cs
 *
 ****************************************************************************/
 
namespace UnrealBuildTool.Rules
{
	public class CriWareEditor : ModuleRules
	{
		public CriWareEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			/* 4.15 - remove PCH includes */
			PrivatePCHHeaderFile = "Private/CriWareEditorPrivatePCH.h";

            DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"AssetTools",
                    "WorkspaceMenuStructure",
				});

            PrivateIncludePaths.AddRange(
				new string[] {
                    "CriWareEditor/Private",
					"CriWareEditor/Private/AssetTools",
                    "CriWareEditor/Private/Factories",
                    "CriWareEditor/Private/ThumbnailRendering",
                    "CriWareEditor/Private/Toolkits",
                    "CriWareEditor/Private/Widgets",
				});

            PublicDependencyModuleNames.AddRange(
				new string[] {
					"AppFramework",
					"Core",
					"CoreUObject",
					"Engine",	
					"SlateCore",
					"Slate",
					"RenderCore",
					"Persona",
					"ContentBrowser",
					"EditorStyle",
					"CriWareRuntime",
					"SharedSettingsWidgets",
                    "DesktopWidgets",
					"SourceControl",
					"InputCore",
					"PropertyEditor",
					"TextureEditor",
					//"InternationalizationSettings",
					"MaterialEditor",
					"AnimationEditor",
					"Kismet",
					"KismetCompiler",
					"GraphEditor",
					"BlueprintGraph",
					"MessageLog",
                    "ToolMenus",
                    "Media",
                    "MediaUtils",
                    "ApplicationCore",
				});

            PrivateDependencyModuleNames.AddRange(
				new string[] {
                    "DesktopPlatform",
					"RHI",
                    "UnrealEd",
				});

			BuildVersion Version;
			if (BuildVersion.TryRead(BuildVersion.GetDefaultFileName(), out Version) && Version.MajorVersion >= 5)
            {
				PrivateDependencyModuleNames.AddRange(
					new string[] {
						"ToolWidgets",
					});
			}

            PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"AssetTools",
                    "WorkspaceMenuStructure",
				});
		}
	}
}