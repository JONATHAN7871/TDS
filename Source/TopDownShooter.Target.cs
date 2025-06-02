// Copyright 2025, CRAFTCODE, All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class TopDownShooterTarget : TargetRules
{
	public TopDownShooterTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;

		ExtraModuleNames.AddRange(new string[] { "TopDownShooter" });

        AdditionalCompilerArguments += " /Zm300";
    }
}
