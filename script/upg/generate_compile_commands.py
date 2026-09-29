import re
from os import PathLike
from pathlib import Path

from upg.setup import setup
from upg.write_project_files import write_project_files
from utility import Git

tag_heuristic = re.compile(r"^(?P<version>(?P<major>\d+)\.(?P<minor>\d+)(\.(?P<patch>\d+))?)(-(?P<tag>.+))?$", flags=re.MULTILINE)

#todo platform/config should be enums from config
def generate_compile_commands(version_or_tag: str, intermediate_dir: Path, platform: str, config: str, pat: str, headers: list[str], modules: list[str]) -> str | Path | PathLike:
    intermediate_dir.mkdir(parents=True, exist_ok=True)
    # Steps:
    # 1. git checkout the unreal branch, Git class should handle making sure we don't do extra work; prevent hooks to
    # prevent issues on earlier versions
    # 1.1. Run/patch setup if needed
    # 2. Generate the project structure in intermediate_dir
    # 3. Patch build tools/environment/config if needed
    # 4. Build the project with the platform and config, with PDB/debug info regardless
    # 5. Return or synthesize compile_commands.json as a string or path to the json file
    # Version specific handling is needed from step 1.1 onwards

    tag_result = tag_heuristic.match(version_or_tag)
    is_tag = tag_result.group("tag") is not None
    patch = tag_result.group("patch")

    if tag_result.group("version") is None:
        raise Exception(f"Version could not be extracted from: {version_or_tag}")

    git = Git(intermediate_dir, "git@github.com:EpicGames/UnrealEngine.git")
    git.checkout(version_or_tag, prevent_hooks=True, is_tag=is_tag)

    if patch is None: patch = "0"
    major = int(tag_result.group("major"))
    minor = int(tag_result.group("minor"))
    patch = int(patch)
    setup(major, minor, patch, git.root, pat)
    write_project_files(major, minor, patch, intermediate_dir, headers, modules)
    pass