using UnrealBuildTool;

public class ChronosHUD : ModuleRules
{
    public ChronosHUD(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UMG", "Slate", "SlateCore", "InputCore" });
        PrivateDependencyModuleNames.AddRange(new[] { "ApplicationCore", "EnhancedInput", "GameplayTags", "NavigationSystem", "ImageCore", "RHI" });
    }
}
