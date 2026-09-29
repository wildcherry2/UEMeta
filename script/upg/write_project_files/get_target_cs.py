def get_target_cs(major: int, minor: int, patch: int):
    if major >= 5 and minor >= 1:
        return """
                    using UnrealBuildTool;

                    public class MetadataHarnessTarget : TargetRules
                    {
                        public MetadataHarnessTarget(TargetInfo Target) : base(Target)
                        {
                            Type = TargetType.Game;
                            ExtraModuleNames.Add("MetadataHarness");
                            DefaultBuildSettings = BuildSettingsVersion.Latest;
                            IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
                        }
                    }
                """
    elif major == 5 or (major == 4 and minor >= 24):
        return """
                    using UnrealBuildTool;

                    public class MetadataHarnessTarget : TargetRules
                    {
                        public MetadataHarnessTarget(TargetInfo Target) : base(Target)
                        {
                            Type = TargetType.Game;
                            ExtraModuleNames.Add("MetadataHarness");
                            DefaultBuildSettings = BuildSettingsVersion.Latest;
                            if (Target.Platform == UnrealTargetPlatform.Win64)
                            {
                                GlobalDefinitions.Add("NTDDI_WIN10_GE=0x0A000010");
                            }
                        }
                    }
                """

    elif (major == 4 and minor <= 4.23) or (major == 4 and minor >= 16):
        return """
                    using UnrealBuildTool;
    
                    public class MetadataHarnessTarget : TargetRules
                    {
                        public MetadataHarnessTarget(TargetInfo Target) : base(Target)
                        {
                            Type = TargetType.Game;
                            ExtraModuleNames.Add("MetadataHarness");
                            if (Target.Platform == UnrealTargetPlatform.Win64)
                            {
                                GlobalDefinitions.Add("NTDDI_WIN10_GE=0x0A000010");
                            }
                        }
                    }
                """

    elif major == 4 and minor < 16:
        return """
                    using UnrealBuildTool;
                    using System.Collections.Generic;

                    public class MetadataHarnessTarget : TargetRules
                    {
                        public MetadataHarnessTarget(TargetInfo Target)
                        {
                            Type = TargetType.Game;
                        }

                        public override void SetupBinaries(
                            TargetInfo Target,
                            ref List<UEBuildBinaryConfiguration> OutBuildBinaryConfigurations,
                            ref List<string> OutExtraModuleNames
                            )
                        {
                            OutExtraModuleNames.Add("MetadataHarness");
                        }
                    }
                """

    raise Exception(f"Unknown Target.cs for version {major}.{minor}.{patch}!")