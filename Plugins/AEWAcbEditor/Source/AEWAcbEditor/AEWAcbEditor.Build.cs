using UnrealBuildTool;

public class AEWAcbEditor : ModuleRules
{
	public AEWAcbEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		CppStandard = CppStandardVersion.Cpp17;
		bEnableExceptions = true;
		PrivateDefinitions.Add("ACBCORE_WITH_UE=1");
		PrivateIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "Private", "Core"));

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "DeveloperSettings" });

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"InputCore",
			"EditorStyle",
			"UnrealEd",
			"ToolMenus",
			"ContentBrowser",
			"ContentBrowserData",
			"AssetTools",
			"AssetRegistry",
			"DesktopPlatform",
			"Projects",
			"Settings",
			"PropertyEditor",
			"CriWareRuntime"
		});
	}
}
