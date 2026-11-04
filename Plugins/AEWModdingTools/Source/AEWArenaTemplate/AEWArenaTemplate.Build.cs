using UnrealBuildTool;

public class AEWArenaTemplate : ModuleRules
{
	public AEWArenaTemplate(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "DeveloperSettings" });
		PrivateDependencyModuleNames.AddRange(new string[] {
			"UnrealEd", "AssetTools", "AssetRegistry", "Slate", "SlateCore", "InputCore", "EditorStyle", "ToolMenus", "Settings", "AEWModdingCore"
		});
	}
}
