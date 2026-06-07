import json
import logging
import re
import subprocess
from pathlib import Path
from re import RegexFlag

from Config import GLOBAL_CONFIG
from Git import init_repo, REPO

def platform_bundled_dotnet_folder():
    match GLOBAL_CONFIG.platform:
        case "win32":
            return "win-x64"
        case "linux":
            return "linux-x64"
        case "darwin":
            return "mac-x64"
        case _:
            raise Exception(f"Unrecognized platform when trying to get bundled dotnet folder name: {GLOBAL_CONFIG.platform}")

def start():
    init_repo()
    if not REPO:
        logging.error("Failed to initialize Repo")
        raise Exception("Failed to initialize Repo")

    repo_root = REPO.working_tree_dir
    if not repo_root:
        logging.error("Failed to initialize Repo (unknown root)")
        raise Exception("Failed to initialize Repo (unknown root)")
    repo_root = Path(repo_root).resolve()

    setup_path = repo_root / f"Setup.{GLOBAL_CONFIG.platform_shell_ext}"
    if not setup_path.exists():
        logging.error(f"Failed to find Setup.{GLOBAL_CONFIG.platform_shell_ext} (unknown path)")
        raise Exception(f"Failed to find Setup.{GLOBAL_CONFIG.platform_shell_ext} (unknown path)")

    with subprocess.Popen(setup_path, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1) as process:
        if process.stdout:
            for line in process.stdout:
                logging.info(line.strip())

    return_code = process.wait()
    if return_code == 0:
        logging.info("Successfully ran Setup script for UnrealEngine.")
    else:
        logging.error("Failed to run Setup script for UnrealEngine.")
        raise Exception("Failed to run Setup script for UnrealEngine.")

    uproject = {
        "FileVersion": 3,
        "EngineAssociation": "",
        "Category": "",
        "Description": "",
        "Modules": [
            {
                "Name": "MetadataHarness",
                "Type": "Runtime",
                "LoadingPhase": "Default"
            }
        ]
    }
    project_root = GLOBAL_CONFIG.intermediate_path / "MetadataHarness"
    project_path = project_root / "MetadataHarness.uproject"
    project_src = project_root / "Source" / "MetadataHarness"
    if project_root.exists():
        project_root.rmdir()
    project_src.mkdir(parents=True, exist_ok=True)

    with open(project_path, "w", encoding="utf-8") as uproject_file:
        json.dump(uproject, uproject_file)

    with open(project_src / "MetadataHarness.build.cs", "w", encoding="utf-8") as build_cs_file:
        build_cs_file.write(f"""
            using UnrealBuildTool;
            public class MetadataHarness : ModuleRules
            {{
                public MetadataHarness(ReadOnlyTargetRules Target) : base(Target)
                {{
                    PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
            
                    PublicDependencyModuleNames.AddRange(new[]
                    {{
                        {", ".join(GLOBAL_CONFIG.public_dependency_module_names)}
                    }});
                }}
            }}
        """)
    with open(project_src / "MetadataHarness.h", "w", encoding="utf-8") as harness_header_file:
        harness_header_file.write("""
            #pragma once
            #include "CoreMinimal.h"
        """)
    with open(project_src / "MetadataHarness.cpp", "w", encoding="utf-8") as harness_src_file:
        harness_src_file.write("""
            #include "MetadataHarness.h"
            IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, MetadataHarness, "MetadataHarness");
        """)
    with open(project_src / "MetadataAnalysis.cpp", "w", encoding="utf-8") as anal_src_file:
        as_includes = {f"#include {header}\n" for header in GLOBAL_CONFIG.headers}
        anal_src_file.write(f"""
            {as_includes}
        """)

    generate_project_files_path = repo_root / f"GenerateProjectFiles.{GLOBAL_CONFIG.platform_shell_ext}"
    if not generate_project_files_path.exists():
        logging.error(f"Failed to find GenerateProjectFiles for UnrealEngine.")
        raise Exception(f"Failed to find GenerateProjectFiles for UnrealEngine.")

    generate_project_files_args = [generate_project_files_path, f"-project=\"{project_path}\""]
    if GLOBAL_CONFIG.platform == "Windows":
        generate_project_files_args.append("-game")
        generate_project_files_args.append("-engine")

    with subprocess.Popen(generate_project_files_args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1) as process:
        if process.stdout:
            for line in process.stdout:
                logging.info(line.strip())

    return_code = process.wait()
    if return_code == 0:
        logging.info("Successfully ran GenerateProjectFiles script for UnrealEngine.")
    else:
        logging.error("Failed to run GenerateProjectFiles script for UnrealEngine.")
        raise Exception("Failed to run GenerateProjectFiles script for UnrealEngine.")

    def entry_is_dotnet(entry: Path):
        return (entry.is_dir()
                and re.search(r"^(\d+\.)+\d+$", entry.name, RegexFlag.M)
                and (entry / platform_bundled_dotnet_folder() / f"dotnet{'.exe' if GLOBAL_CONFIG.platform == "win32" else ''}").exists())

    bundled_dotnet = repo_root / "Engine" / "Binaries" / "ThirdParty" / "DotNet"
    if not bundled_dotnet.exists():
        logging.error("Failed to find ThirdParty/DotNet for UnrealEngine.")
        raise Exception("Failed to find ThirdParty/DotNet for UnrealEngine.")
    bundled_dotnet: list[Path] = [entry for entry in bundled_dotnet.iterdir() if entry_is_dotnet(entry)]
    bundled_dotnet.sort(reverse=True)
    if len(bundled_dotnet) == 0:
        logging.error("Failed to find bundled DotNet for UnrealEngine.")
        raise Exception("Failed to find bundled DotNet for UnrealEngine.")
    bundled_dotnet: Path = (bundled_dotnet[0] / platform_bundled_dotnet_folder() / f"dotnet{'.exe' if GLOBAL_CONFIG.platform == "win32" else ''}").resolve()
    ubt_path = repo_root / "Engine" / "Binaries" / "DotNET" / "UnrealBuildTool" / "UnrealBuildTool.dll"
    if not ubt_path.exists():
        logging.error("Failed to find UnrealBuildTool for UnrealEngine.")
        raise Exception("Failed to find UnrealBuildTool for UnrealEngine.")
    ubt_args = [bundled_dotnet, f"\"{ubt_path}\"", "MetadataHarness", "Win64", "Shipping", f"-project=\"{project_path}\"",
                "-WaitMutex", "-architecture=x64", f"-WorkingDir=\"{project_root / "Intermediate" / "ProjectFiles"}\"",
                f"-Files=\"{project_src / "MetadataAnalysis.cpp"}\""]
    with subprocess.Popen(ubt_args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1) as ubt_process:
        if ubt_process.stdout:
            for line in ubt_process.stdout:
                logging.info(line.strip())
    return_code = ubt_process.wait()
    if return_code == 0:
        logging.info("Successfully ran UBT/UHT for project.")
    else:
        logging.error("Failed to run UBT/UHT for project.")
        raise Exception("Failed to run UBT/UHT for project.")

    generate_project_files_args = [generate_project_files_path, "-Mode=GenerateClangDatabase", "MetadataHarness",
                                   "Win64", "Shipping", f"-project=\"{project_path}\""]
    with subprocess.Popen(generate_project_files_args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1) as process:
        if process.stdout:
            for line in process.stdout:
                logging.info(line.strip())

    return_code = process.wait()
    compile_commands_path = repo_root / "compile_commands.json"
    if return_code == 0 and compile_commands_path.exists():
        logging.info("Successfully generated compile_commands.json for UnrealEngine.")
    else:
        logging.error("Failed to generate compile_commands.json for UnrealEngine.")
        raise Exception("Failed to run compile_commands.json for UnrealEngine.")