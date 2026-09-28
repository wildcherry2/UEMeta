from os import PathLike
from pathlib import Path

from utility.Git import Git


#todo platform/config should be enums from config
def generate_uproject(branch_or_tag: str, intermediate_dir: Path, platform: str, config: str) -> str | Path | PathLike:
    intermediate_dir.mkdir(parents=True, exist_ok=True)
    # Steps:
    # 1. git checkout the unreal branch, Git class should handle making sure we don't do extra work; prevent hooks to
    # prevent issues on earlier versions
    # 1.1. Run/patch setup if needed
    # 2. Generate the project structure in intermediate_dir
    # 3. Build the project with the platform and config, with PDB/debug info regardless
    # 4. Return or synthesize compile_commands.json as a string or path to the json file
    # Version specific handling is needed from step 1.1 onwards
    git = Git(intermediate_dir, "git@github.com:EpicGames/UnrealEngine.git")
    #todo figure out if branch or tag to set kwarg value properly
    git.checkout(branch_or_tag, prevent_hooks=True)
    pass