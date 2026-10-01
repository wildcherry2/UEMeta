import json
import shutil
import sys
from pathlib import Path

from upg.write_project_files.get_build_cs import get_build_cs
from upg.write_project_files.get_metadata_harness_h import get_metadata_harness_h
from upg.write_project_files.get_target_cs import get_target_cs
from upg.write_project_files.get_uproject import get_uproject
from upg.write_project_files.get_metadata_analysis import get_metadata_analysis
from utility import execute

def write_project_files(major: int, minor: int, patch: int, intermediate_dir: Path, headers: list[str], modules: list[str]):
    project_root = intermediate_dir / "MetadataHarness"
    uproject = project_root / "MetadataHarness.uproject"
    project_sources = project_root / "Source" / "MetadataHarness"
    target_cs = project_root / "Source" / "MetadataHarness.Target.cs"
    build_cs = project_sources / "MetadataHarness.Build.cs"
    harness_header = project_sources / "MetadataHarness.h"
    harness_cpp = project_sources / "MetadataHarness.cpp"
    analysis_cpp = project_sources / "MetadataAnalysis.cpp"

    if project_root.exists():
        shutil.rmtree(project_root)

    project_sources.mkdir(parents=True, exist_ok=True)

    with open(uproject, "w", encoding="utf-8") as uproject_file:
        uproject_file.write(json.dumps(get_uproject(major, minor, patch)))

    with open(target_cs, "w", encoding="utf-8") as target_cs_file:
        target_cs_file.write(get_target_cs(major, minor, patch))

    with open(build_cs, "w", encoding="utf-8") as build_cs_file:
        build_cs_file.write(get_build_cs(major, minor, patch, modules))

    with open(harness_header, "w", encoding="utf-8") as harness_header_file:
        harness_header_file.write(get_metadata_harness_h(major, minor))

    with open(harness_cpp, "w", encoding="utf-8") as harness_cpp_file:
        harness_cpp_file.write("""
            #include "MetadataHarness.h"
            IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, MetadataHarness, "MetadataHarness");
        """)

    with open(analysis_cpp, "w", encoding="utf-8") as analysis_cpp_file:
        analysis_cpp_file.write(get_metadata_analysis(major, minor, patch, headers))

    unreal_root = intermediate_dir / "UnrealEngine" #todo pass in git root
    generate_project_files_script = unreal_root / f"GenerateProjectFiles.{"bat" if sys.platform == "win32" else "sh"}"

    execute([generate_project_files_script, f"-project={uproject}", "-game", "engine"])

    return uproject