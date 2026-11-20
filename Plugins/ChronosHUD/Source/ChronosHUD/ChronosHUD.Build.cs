using UnrealBuildTool;

public class ChronosHUD : ModuleRules
{
    public ChronosHUD(ReadOnlyTargetRules Target) : base(Target)
    {
                // Each UI implementation has its own file-local constants and helpers.
        bUseUnity = false;
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UMG", "Slate", "SlateCore", "InputCore" });
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PrivateDependencyModuleNames.Add("InputDevice");
            PublicSystemLibraries.AddRange(new[] { "hid.lib", "setupapi.lib" });
        }
        PrivateDependencyModuleNames.AddRange(new[] { "ApplicationCore", "EnhancedInput", "GameplayTags", "NavigationSystem", "ImageCore", "RHI", "RenderCore", "ChronosGuardado" });
    }
}
