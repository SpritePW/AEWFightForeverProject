/****************************************************************************
 *
 * CRI Middleware SDK
 *
 * Copyright (c) 2017-2021 CRI Middleware Co., Ltd.
 *
 * Library  : CRIWARE plugin for Unreal Engine 4
 * Module   : Build Settings of CriWareSequencer Module
 * File     : CriWareCompositingEditor.Build.cs
 *
 ****************************************************************************/
namespace UnrealBuildTool.Rules
{
	public class CriWareSequencer : ModuleRules
	{
		public CriWareSequencer(ReadOnlyTargetRules Target) : base(Target)
        {
			PrivateIncludePaths.AddRange(
				new string[] {
					"CriWareSequencer/Private",
					"CriWareSequencer/Private/Sequencer",
					"CriWareSequencer/Private/Shared",
				}
			);
	
	        PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"UnrealEd",
					"EditorStyle",
					"SlateCore",
					"Slate",
					"AssetTools",
					"MovieScene",
					"MovieSceneTools",
					"MovieSceneTracks",
					"PropertyEditor",
					"Sequencer",
					"LevelSequence",
					"SharedSettingsWidgets",
					"DesktopWidgets",
					"CriWareRuntime",
					"RenderCore",
					"RHI",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Renderer",
                    "CriWareMovieScenes"
				}
			);
		}
	}
}
