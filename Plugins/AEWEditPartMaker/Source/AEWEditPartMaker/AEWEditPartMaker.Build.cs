using UnrealBuildTool;

public class AEWEditPartMaker : ModuleRules
{
	public AEWEditPartMaker(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "DeveloperSettings" });

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"InputCore",
			"EditorStyle",
			"UnrealEd",
			"ToolMenus",
			"PropertyEditor",
			"ContentBrowser",
			"AssetRegistry",
			"Projects",
			"Settings"
		});
	}
}
