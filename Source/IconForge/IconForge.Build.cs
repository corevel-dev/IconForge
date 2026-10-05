using UnrealBuildTool;

public class IconForge : ModuleRules
{
	public IconForge(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateIncludePaths.Add(ModuleDirectory + "/Private");

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate", "SlateCore", "InputCore", "ApplicationCore",
			"UnrealEd", "EditorFramework", "ToolMenus",
			"ContentBrowser", "ContentBrowserData", "AssetRegistry", "PropertyEditor",
			"RenderCore", "RHI", "ImageCore", "ImageWrapper",
			"Json", "JsonUtilities"
		});
	}
}
