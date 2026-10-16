/****************************************************************************
 *
 * CRI Middleware SDK
 *
 * Copyright (c) 2021 CRI Middleware Co., Ltd.
 *
 * Library  : CRIWARE plugin for Unreal Engine 4
 * Module   : Build Settings of CriWareMovieScenes Module
 * File     : CriWareMovieScenes.Build.cs
 *
 ****************************************************************************/
namespace UnrealBuildTool.Rules
{
	public class CriWareMovieScenes : ModuleRules
	{
		public CriWareMovieScenes(ReadOnlyTargetRules Target) : base(Target)
        {

			PrivateIncludePaths.AddRange(
				new string[] {
                    "CriWareMovieScenes/Private",
                    "CriWareMovieScenes/Private/MovieScenes",
            	}
			);

	        PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"MovieScene",
					"MovieSceneTracks",
					"TimeManagement",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"RenderCore",
					"RHI",
					"CriWareRuntime",
                }
			);
		}
	}
}
