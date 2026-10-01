import re
import sys
from pathlib import Path
from utility import log_exc

def get_bundled_dotnet(major: int, minor: int, patch: int, unreal_repo: Path):
    dotnet_root = unreal_repo / "Engine" / "Binaries" / "ThirdParty" / "DotNet"
    if not dotnet_root.exists():
        log_exc("Failed to find ThirdParty/DotNet for UnrealEngine.")

    dotnet_exe = f"dotnet{'.exe' if sys.platform == 'win32' else ''}"
    platform_dict = get_platform_dict(major, minor, patch)

    def entry_is_dotnet(entry: Path):
        return (entry.is_dir()
                and re.search(r"^(\d+\.)+\d+$", entry.name, re.RegexFlag.M)
                and (entry / platform_dict[sys.platform] / dotnet_exe).exists())

    def dotnet_version(entry: Path) -> tuple[int, ...]:
        return tuple(int(part) for part in entry.name.split("."))

    bundled_dotnet_versions: list[Path] = [entry for entry in dotnet_root.iterdir() if entry_is_dotnet(entry)]
    bundled_dotnet_versions.sort(key=dotnet_version, reverse=True)
    if len(bundled_dotnet_versions) == 0:
        log_exc("Failed to find bundled DotNet for UnrealEngine.")
    return (bundled_dotnet_versions[0] / platform_dict[sys.platform] / dotnet_exe).resolve()

def get_platform_dict(major: int, minor: int, patch: int):
    if major == 5 and minor >= 5:
        return {"win32": "win-x64", 'linux': 'linux-x64', 'darwin': 'mac-x64'}
    if major == 5 and (minor == 3 or minor == 4):
        return {'linux': 'linux', 'darwin': 'mac-x64', 'win32': 'windows'}
    return {'linux': 'Linux', 'darwin': 'Mac', 'win32': 'Windows'}