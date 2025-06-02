// Copyright 2025, CRAFTCODE, All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class TopDownShooterEditorTarget : TargetRules
{
	public TopDownShooterEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;

		ExtraModuleNames.AddRange( new string[] { "TopDownShooter" } );
	}
}
