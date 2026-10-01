from pathlib import Path
from shlex import join as shell_join, quote as shell_quote
from subprocess import list2cmdline

import sys

from upg.build.get_bundled_dotnet import get_bundled_dotnet
from upg.build.get_ubt_path import get_ubt_path
from utility import execute, ExecuteOutputOptions


def build(major: int, minor: int, patch: int, unreal_dir: Path, uproject_file: Path, platform: str, config: str, inject_vars_script: Path):
    dotnet = get_bundled_dotnet(major, minor, patch, unreal_dir)
    ubt = get_ubt_path(major, unreal_dir)
    args = [
        str(dotnet),
        str(ubt),
        "MetadataHarness",
        platform,
        config,
        f"-project={uproject_file}",
        "-WaitMutex",
        "-architecture=x64",
    ]
    if sys.platform == "win32":
        # CALL keeps the batch script's environment available to the build command.
        setup_command = list2cmdline([str(inject_vars_script), "x64"])
        command = f"call {setup_command} && {list2cmdline(args)}"
    else:
        command = f"{shell_quote(str(inject_vars_script))} && {shell_join(args)}"

    execute(command,
            success_msg=f"Successfully ran UBT/UHT for branch {(major, minor, patch)}.",
            fail_msg=f"Failed to run UBT/UHT for branch {(major, minor, patch)}.",
            output=ExecuteOutputOptions.FILE | ExecuteOutputOptions.STDOUT,
            shell=True,
            addl_env={"MSBUILDDISABLENODEREUSE": "1", "UseSharedCompilation": "false"})
