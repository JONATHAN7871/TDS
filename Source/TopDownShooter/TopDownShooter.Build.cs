// Copyright 2025, CRAFTCODE, All Rights Reserved.

using UnrealBuildTool;

public class TopDownShooter : ModuleRules
{
    public TopDownShooter(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "OnlineSubsystem",
            "OnlineSubsystemUtils",
            "Networking",
            "Sockets",
            "NetCore",
            "GameplayAbilities",
            "GameplayTags",
            "GameplayTasks"
        });

        PrivateDependencyModuleNames.AddRange(new string[] { });

        PublicIncludePaths.AddRange(new string[] {
            "TopDownShooter/Abilities/Movement",
            "TopDownShooter/Abilities/Movement/MovementState",
            "TopDownShooter/Core/Characters",
            "TopDownShooter/Core/Components",
            "TopDownShooter/Core/Controllers",
            "TopDownShooter/Core/GamePlay",
            "TopDownShooter/Core/HUD",
            "TopDownShooter/Camera"
        });

        // ����������� ����������� ������ ��� OnlineSubsystem (��������, Steam)
        DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");
    }
}
