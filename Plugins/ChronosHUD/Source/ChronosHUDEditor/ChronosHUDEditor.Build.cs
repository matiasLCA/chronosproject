using UnrealBuildTool;

public class ChronosHUDEditor : ModuleRules
{
    public ChronosHUDEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UnrealEd", "BlueprintGraph", "ChronosHUD", "EnhancedInput", "InputBlueprintNodes", "AssetRegistry", "InputCore", "UMG" });
        PrivateIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "..", "ChronosHUD", "Private"));
    }
}
