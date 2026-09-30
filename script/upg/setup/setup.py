import asyncio
import shutil
import sys
import tarfile
import zipfile
from pathlib import Path
from upg.setup.constants import LOCAL_GIT_DEPS, CDN_DEPS, REMOTE_GIT_DEPS
from utility import execute
from urllib.request import Request, urlopen

# versions 4.11 to 5.1 (patches included) have broken gitdeps that need replacement before running setup
# versions 4.6 to 4.10 (patches included) have broken gitdeps and use manually extracted dependencies
# prior versions have their dependencies in the release assets
# versions 5.2+ have working setup scripts

# Setup refers to the step of running the setup script after unreal checks out/clones, usually done via git hooks,
# but it's very buggy on a majority of versions that are more than a year old so we manually patch versions < 5.2
def setup(major: int, minor: int, patch: int, repo_root: Path, pat: str,
          env: dict[str, str] | None = None):
    if (major, minor) >= (5, 2):
        run_setup_script(repo_root, env)
    elif (major == 4 and minor >= 11) or (major == 5 and minor < 2):
        patch_gitdeps(major, minor, patch, repo_root)
        run_setup_script(repo_root, env)
    elif major == 4 and 6 <= minor < 11:
        path = asyncio.run(download_deps_from_cdn(major, minor, repo_root.parent / "zips"))
        untar(path, repo_root)
    elif major == 4 and minor < 6:
        for path in download_deps_from_github(major, minor, patch, repo_root.parent / "zips", pat):
            unzip(path, repo_root)
    else:
        raise Exception(f"Unhandled version {major}-{minor}-{patch}")

def run_setup_script(root: Path, env: dict[str, str] | None = None):
    if sys.platform == "win32":
        execute([root / "Setup.bat", "--force"], cwd=root, addl_env=env)
    elif sys.platform == "linux":
        execute([root / "Setup.sh", "--force"], cwd=root, addl_env=env)

def patch_gitdeps(major: int, minor: int, patch: int, root: Path):
    path = LOCAL_GIT_DEPS[major][minor][patch]
    if path is None:
        raise Exception(f"No local gitdeps for version {major}.{minor}.{patch}")
    target = root / "Engine" / "Build" / "Commit.gitdeps.xml"
    if not target.exists() or not target.is_file():
        raise Exception(f"No repo gitdeps for version {major}.{minor}.{patch}")
    shutil.copy2(path, target)

# fetch from mega
async def download_deps_from_cdn(major: int, minor: int, cache_dir: Path) -> Path:
    out_path = cache_dir / f"{major}-{minor}.tar.zst"
    if out_path.exists(): return out_path
    from mega.client import MegaNzClient
    if not cache_dir.exists(): cache_dir.mkdir(exist_ok=True, parents=True)

    url = CDN_DEPS[major][minor]
    if url is None:
        raise Exception(f"No CDN deps for version {major}.{minor}!")
    async with MegaNzClient() as client:
        handle, key = client.parse_file_url(url)
        path = await client.download_public_file(handle, key, cache_dir)
        return path.rename(out_path)

# fetch from github
def download_deps_from_github(major: int, minor: int, patch: int, cache_dir: Path, pat: str) -> list[Path]:
    urls = REMOTE_GIT_DEPS[major][minor][patch]
    if not urls:
        raise Exception(f"No known remote git deps for version {major}.{minor}!")
    version_cache = cache_dir / f"{major}.{minor}.{patch}"
    version_cache.mkdir(exist_ok=True, parents=True)
    paths = []
    for index, url in enumerate(urls, start=1):
        out_path = version_cache / f"Required_{index}of{len(urls)}.zip"
        if not out_path.exists():
            if not pat:
                raise ValueError("A GitHub PAT is required to fetch older Unreal dependencies.")
            headers = {"Accept": "application/octet-stream", "User-Agent": "curl", "Authorization": f"token {pat}"}
            with urlopen(Request(url, headers=headers)) as response:
                with open(out_path, "wb") as download_file:
                    shutil.copyfileobj(response, download_file)
        paths.append(out_path)
    return paths

def untar(in_tar_zst: Path, out_dir: Path):
    if not in_tar_zst.exists() or not in_tar_zst.is_file():
        raise FileNotFoundError(f"{in_tar_zst} is not an existing file!")
    if in_tar_zst.suffix != ".zst":
        raise Exception(f"{in_tar_zst} is not a tar.zst file!")
    if not out_dir.exists() or not out_dir.is_dir():
        raise NotADirectoryError(f"{out_dir} is not an existing directory!")

    with tarfile.open(in_tar_zst, mode="r:zst") as tar:
        tar.extractall(out_dir)

def unzip(in_zip: Path, out_dir: Path):
    if not in_zip.exists() or not in_zip.is_file():
        raise FileNotFoundError(f"{in_zip} is not an existing file!")
    if not in_zip.suffix == ".zip":
        raise Exception(f"{in_zip} is not a zip file!")
    if not out_dir.exists() or not out_dir.is_dir():
        raise NotADirectoryError(f"{out_dir} is not an existing directory!")

    with zipfile.ZipFile(in_zip, mode="r") as zip:
        zip.extractall(out_dir)