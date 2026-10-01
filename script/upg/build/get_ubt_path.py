from pathlib import Path

def get_ubt_path(major: int, unreal_repo: Path):
    if major == 5:
        return unreal_repo / "Engine" / "Binaries" / "DotNET" / "UnrealBuildTool" / "UnrealBuildTool.dll"
    return unreal_repo / "Engine" / "Binaries" / "DotNET" / "UnrealBuildTool.exe"