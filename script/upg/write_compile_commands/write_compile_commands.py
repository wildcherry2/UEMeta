import json
import sys
from pathlib import Path

from utility import execute


def write_compile_commands(major: int, minor: int, patch: int, unreal_repo: Path, uproject: Path, platform: str, config: str):
    generate_project_files = unreal_repo / f"GenerateProjectFiles.{"bat" if sys.platform == "win32" else "sh"}"
    if major == 5:
        execute([generate_project_files, "-Mode=GenerateClangDatabase", "MetadataHarness", platform, config, f"-project={uproject}"])
        return filter_compile_commands(unreal_repo / "compile_commands.json")

    raise NotImplementedError()

def filter_compile_commands(cc_path: Path):
    with open(cc_path, 'r+') as cc_file:
        cc: list[dict[str, str]] = json.load(cc_file)
        cc = [entry for entry in cc if entry["file"].endswith("MetadataAnalysis.cpp") ]
        cc_file.seek(0)
        cc_file.write(json.dumps(cc))
        cc_file.truncate()
    return cc_path