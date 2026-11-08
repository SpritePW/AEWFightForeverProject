using UnrealBuildTool;

public class AEWModdingCore : ModuleRules
{
	public AEWModdingCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "DeveloperSettings", "Slate", "SlateCore", "InputCore", "ToolMenus" });
		PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "EditorStyle", "PropertyEditor", "Settings", "AssetTools", "AssetRegistry", "FileUtilities" });
	}
}
