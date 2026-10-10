using UnrealBuildTool;

public class FastGeoDistanceCullingEditor : ModuleRules
{
    public FastGeoDistanceCullingEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "FastGeoDistanceCulling"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "FastGeoStreaming",
                "UnrealEd"
            }
        );
    }
}