
def get_build_cs(major: int, minor: int, patch: int, modules: list[str]):
    module_names = ", ".join(f'"{name}"' for name in modules)
    if major == 4 and minor <= 15:
        return f"""
                    using UnrealBuildTool;
                    public class MetadataHarness : ModuleRules
                    {{
                        public MetadataHarness(TargetInfo Target)
                        {{
                            PCHUsage = PCHUsageMode.NoSharedPCHs;
                            MinFilesUsingPrecompiledHeaderOverride = 999999;
                            Definitions.Add("NTDDI_WIN10_GE=0x0A000010");

                            PublicDependencyModuleNames.AddRange(new string[]
                            {{
                                {module_names}
                            }});
                        }}
                    }}
                """
    else:
        return f"""
            using UnrealBuildTool;
            public class MetadataHarness : ModuleRules
            {{
                public MetadataHarness(ReadOnlyTargetRules Target) : base(Target)
                {{
                    PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

                    PublicDependencyModuleNames.AddRange(new string[]
                    {{
                        {module_names}
                    }});
                }}
            }}
        """