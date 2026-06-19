import json
import logging
import os
import re
import shutil
import tarfile
import time
import urllib.error
import urllib.parse
import urllib.request
import zipfile
from pathlib import Path, PurePosixPath
from re import RegexFlag
from typing import Callable, Final

import compression.zstd as zstd

from GlobalUtil import ExecuteOutputOptions, execute, log_exc
from unreal.Constants import CANONICAL_BRANCH_RE

VS2013_ENV: dict[str, str] | None = None
VS2015_ENV: dict[str, str] | None = None
GITHUB_PAT: str | None = None
TAR_ZST_EXTRACT_LOG_INTERVAL_SECONDS: Final[int] = 15
TAR_ZST_COPY_BUFFER_SIZE: Final[int] = 1024 * 1024


def get_broken_gitdep_branches():
    directory = Path.cwd() / "external"
    if not directory.exists() or not directory.is_dir():
        raise Exception(f"Could not construct BrokenBranchesDict because directory {directory} does not exist!")

    bbr = re.compile(r".*Commit\.gitdeps\.(?P<branch>(?P<major>\d+\.\d+)(\.(?P<patch>\d+))?).xml$",
                     RegexFlag.M | RegexFlag.U)
    out: dict[str, Path] = {}
    for file in directory.iterdir():
        match = bbr.match(file.name)
        if match is not None:
            groups = match.groupdict()
            if groups["patch"] is None or groups["patch"] == "0":
                out[groups["major"]] = file
            out[groups["branch"]] = file
            out[groups["branch"] + "-release"] = file

    return out

class CanonicalVersion:
    def __init__(self, branch: str):
        match = CANONICAL_BRANCH_RE.match(branch)
        if match is None:
            # we could add custom mappings for certain branches/tags here
            raise Exception(f"Failed to parse CanonicalBranch: {branch}")
        match = match.groupdict()
        self.branch = branch
        self.version: Final[str] = match['version']
        self.major: Final[str] = match['major']
        self.minor: Final[str] = match['minor']
        if "patch" in match:
            self.patch: Final[str | None] = match["patch"]
        else:
            self.patch: Final[str | None] = None

        if "label" in match:
            self.label: Final[str | None] = match["label"]
        else:
            self.patch: Final[str | None] = None

    def __eq__(self, value, /) -> bool:
        if isinstance(value, CanonicalVersion):
            return self.version == value.version and self.label == value.label
        return self.__eq__(CanonicalVersion(str(value)))

    def __str__(self):
        return self.version

    def is_same_version(self, other: str | CanonicalVersion) -> bool:
        if isinstance(other, CanonicalVersion):
            return other.version == self.version
        return self.is_same_version(CanonicalVersion(other))

    def is_same_majmin(self, other: str | CanonicalVersion) -> bool:
        if isinstance(other, CanonicalVersion):
            return other.major == self.major and other.minor == self.minor
        return self.is_same_majmin(CanonicalVersion(other))

    def get_majmin(self):
        return self.major + "." + self.minor

    def get_trimmed_version(self):
        if self.patch is None or self.patch == "0":
            return self.get_majmin()
        return self.get_majmin() + "." + self.patch


def validate_msvc():
    vswhere = os.environ.get("ProgramFiles(x86)")
    if not vswhere:
        raise Exception("ProgramFiles(x86) not found!")
    vswhere = Path(vswhere)
    if not vswhere.exists():
        raise Exception("ProgramFiles(x86) not found!")
    vswhere = vswhere / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if not vswhere.exists():
        raise Exception("vswhere.exe not found! Is Visual Studio 2022 installed?"
                        "If not, use the unreal editor; download an engine version in the epic launcher,"
                        "create a new project, set it to C++, and it'll give you a valid download.")

    _, jsn = execute([vswhere.resolve(), "-utf8", "-format", "json", "-nocolor"],
                     success_msg="Acquired VS version info...",
                     fail_msg="Failed to get VS version info!",
                     expected_ret=0,
                     output=ExecuteOutputOptions.SILENT)

    if len(jsn) == 0:
        log_exc("vswhere.exe failed to dump json!")

    try:
        jsn_dict: list[dict[str, str]] = json.loads(jsn)
    except Exception as e:
        log_exc(f"Failed to parse json from vswhere: {e}")

    if len(jsn_dict) == 0:
        log_exc("vswhere.exe failed to dump json!")
    if not any(re.search(r"Visual Studio (Community|Professional|Enterprise) 2022", install['displayName']) for install in jsn_dict):
        log_exc("Visual Studio 2022 install not found, which is required for Unreal Engine! To install it, "
                "download an engine version in the epic launcher, "
                "create a new project, set it to C++, and it'll give you a valid download.")
    logging.info("Validated VS 2022 install!")


def path_value(path: Path) -> str:
    value = str(path)
    return value if value.endswith(("\\", "/")) else value + os.sep


def env_get(env: dict[str, str], key: str) -> str | None:
    for env_key, value in env.items():
        if env_key.upper() == key.upper():
            return value
    return None


def env_set(env: dict[str, str], key: str, value: str) -> None:
    existing_key = next((env_key for env_key in env if env_key.upper() == key.upper()), None)
    if existing_key is not None and existing_key != key:
        env.pop(existing_key)
    env[key] = value


def path_from_value(value: Path | str | None) -> Path | None:
    if value is None:
        return None
    if isinstance(value, Path):
        return value
    value = value.strip().strip('"')
    return Path(value) if value else None


def is_installed_product(display_name: str) -> bool:
    try:
        import winreg
    except ImportError:
        return False

    display_name_lower = display_name.lower()
    uninstall_key = r"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall"
    for hive in (winreg.HKEY_LOCAL_MACHINE, winreg.HKEY_CURRENT_USER):
        for access in (winreg.KEY_WOW64_32KEY, winreg.KEY_WOW64_64KEY, 0):
            try:
                with winreg.OpenKey(hive, uninstall_key, 0, winreg.KEY_READ | access) as key:
                    for index in range(winreg.QueryInfoKey(key)[0]):
                        try:
                            with winreg.OpenKey(key, winreg.EnumKey(key, index)) as sub_key:
                                value, _ = winreg.QueryValueEx(sub_key, "DisplayName")
                        except OSError:
                            continue

                        if display_name_lower in str(value).lower():
                            return True
            except OSError:
                pass
    return False


def get_vs_vcvarsall(version: str, override: Path | None = None) -> Path:
    labels = {"12.0": "VS2013", "14.0": "VS2015"}
    common_tools_vars = {"12.0": "VS120COMNTOOLS", "14.0": "VS140COMNTOOLS"}
    override_vars = {
        "12.0": ("UEMETA_VS2013_VCVARSALL", "VS2013_VCVARSALL", "VS120VCVARSALL"),
        "14.0": ("UEMETA_VS2015_VCVARSALL", "VS2015_VCVARSALL", "VS140VCVARSALL"),
    }
    override_args = {"12.0": "[Unreal Parser] vs2013_vcvarsall", "14.0": "UEMETA_VS2015_VCVARSALL"}
    script_names = ("vcvarsall.bat", "vcbuildtools.bat")
    label = labels.get(version, f"VS{version}")
    vc_dir_candidates: list[Path] = []
    script_candidates: list[Path] = []

    def add_vc_dir_candidate(candidate: Path | str | None):
        candidate = path_from_value(candidate)
        if candidate is not None:
            vc_dir_candidates.append(candidate)

    def add_script_candidate(candidate: Path | str | None):
        candidate = path_from_value(candidate)
        if candidate is not None:
            script_candidates.append(candidate)

    def add_path_candidate(candidate: Path | str | None):
        candidate = path_from_value(candidate)
        if candidate is None:
            return

        if candidate.suffix.lower() == ".bat":
            add_script_candidate(candidate)
            return

        for script_name in script_names:
            add_script_candidate(candidate / script_name)
        add_vc_dir_candidate(candidate)

    def add_vc_dir_from_common_tools(common_tools: str | None):
        if common_tools:
            add_vc_dir_candidate(Path(common_tools) / ".." / ".." / "VC")

    def add_vc_dir_from_install_dir(install_dir: str | None):
        if install_dir:
            add_vc_dir_candidate(Path(install_dir) / ".." / ".." / "VC")

    def add_vc_dir_from_vs_root(vs_root: str | None):
        if vs_root and version in str(Path(vs_root)):
            add_vc_dir_candidate(Path(vs_root) / "VC")

    def add_existing_scripts_under(root: Path):
        if not root.exists():
            return
        try:
            for script_name in script_names:
                for script in root.rglob(script_name):
                    add_script_candidate(script)
        except OSError:
            pass

    add_path_candidate(override)
    for env_var in override_vars.get(version, ()):
        add_path_candidate(os.environ.get(env_var))
    add_vc_dir_from_common_tools(os.environ.get(common_tools_vars.get(version, "")))
    add_vc_dir_from_vs_root(os.environ.get("VSINSTALLDIR"))

    try:
        import winreg

        for hive in (winreg.HKEY_LOCAL_MACHINE, winreg.HKEY_CURRENT_USER):
            for access in (winreg.KEY_WOW64_32KEY, winreg.KEY_WOW64_64KEY, 0):
                try:
                    with winreg.OpenKey(hive, r"SOFTWARE\Microsoft\VisualStudio\SxS\VC7", 0,
                                        winreg.KEY_READ | access) as key:
                        value, _ = winreg.QueryValueEx(key, version)
                        add_vc_dir_candidate(Path(value))
                except OSError:
                    pass

                for product in ("VisualStudio", "VCExpress", "WDExpress"):
                    key_root = rf"SOFTWARE\Microsoft\{product}\{version}"
                    for key_path, value_name, add_value in (
                            (key_root, "InstallDir", add_vc_dir_from_install_dir),
                            (key_root + r"\Setup\VC", "ProductDir", add_vc_dir_candidate),
                            (key_root + r"\Setup\VS", "ProductDir", add_vc_dir_from_vs_root),
                    ):
                        try:
                            with winreg.OpenKey(hive, key_path, 0,
                                                winreg.KEY_READ | access) as key:
                                value, _ = winreg.QueryValueEx(key, value_name)
                                add_value(value)
                        except OSError:
                            pass
    except ImportError:
        pass

    program_files_roots = [value for value in (os.environ.get("ProgramFiles(x86)"), os.environ.get("ProgramFiles")) if value]
    for program_files_root in program_files_roots:
        root = Path(program_files_root)
        add_vc_dir_candidate(root / f"Microsoft Visual Studio {version}" / "VC")
        add_existing_scripts_under(root / f"Microsoft Visual Studio {version}")
        if version == "12.0":
            add_existing_scripts_under(root / "Microsoft Visual C++ Build Tools 2013")
            add_existing_scripts_under(root / "Microsoft Build Tools" / "12.0")

    for candidate in vc_dir_candidates:
        for script_name in script_names:
            add_script_candidate(candidate / script_name)

    seen: set[Path] = set()
    for candidate in script_candidates:
        resolved_candidate = candidate.resolve()
        if resolved_candidate in seen:
            continue
        seen.add(resolved_candidate)

        if resolved_candidate.exists():
            return resolved_candidate

    msbuild_only_note = ""
    if version == "12.0" and is_installed_product("Microsoft Build Tools 2013"):
        msbuild_only_note = (
            " Microsoft Build Tools 2013 is installed, but no VS2013 C++ environment batch was found; "
            "that package is not enough for UE 4.5 unless a separate VS2013 C++ toolchain is also installed."
        )

    log_exc(f"Failed to find the {label} C++ toolchain. Expected "
            f"C:\\Program Files (x86)\\Microsoft Visual Studio {version}\\VC\\vcvarsall.bat, "
            f"the matching VisualStudio\\SxS\\VC7\\{version} registry value, or "
            f"{common_tools_vars.get(version, 'VSxxCOMNTOOLS')} pointing at Common7\\Tools. "
            f"If the standalone tools are installed somewhere else, pass {override_args.get(version, 'a vcvarsall override')}."
            f"{msbuild_only_note}")


def get_vs2015_vcvarsall() -> Path:
    return get_vs_vcvarsall("14.0")


def get_vs2013_vcvarsall(override: Path | None = None) -> Path:
    return get_vs_vcvarsall("12.0", override)


def capture_vcvarsall_env(vcvarsall: Path, label: str, required_vars: list[str]) -> dict[str, str]:
    _, output = execute(["cmd.exe", "/d", "/c", "call", vcvarsall, "x64", "&&", "set"],
                        success_msg=f"Captured {label} build environment from {vcvarsall}.",
                        fail_msg=f"Failed to capture {label} build environment from {vcvarsall}!",
                        output=ExecuteOutputOptions.SILENT)

    env: dict[str, str] = {}
    for line in output.splitlines():
        key, separator, value = line.partition("=")
        if separator == "" or key.startswith("="):
            continue
        env[key] = value

    env_vars = {key.upper(): value for key, value in env.items()}
    missing = [var for var in required_vars if not env_vars.get(var)]
    if missing:
        log_exc(f"{label} vcvarsall did not provide required environment variables: {', '.join(missing)}")

    return env


def derive_vc_dir_from_env(vcvarsall: Path, env: dict[str, str]) -> Path | None:
    vc_install_dir = env_get(env, "VCINSTALLDIR")
    if vc_install_dir:
        return Path(vc_install_dir)

    for parent in vcvarsall.parents:
        if parent.name.lower() == "vc":
            return parent
    return None


def ensure_vs_common_tools_env(version: str, vcvarsall: Path, env: dict[str, str]) -> None:
    common_tools_vars = {"12.0": "VS120COMNTOOLS", "14.0": "VS140COMNTOOLS"}
    common_tools_var = common_tools_vars[version]
    if env_get(env, common_tools_var):
        return

    vc_dir = derive_vc_dir_from_env(vcvarsall, env)
    if vc_dir is None:
        return

    if not env_get(env, "VCINSTALLDIR"):
        env_set(env, "VCINSTALLDIR", path_value(vc_dir))

    if vc_dir.name.lower() == "vc":
        vs_root = vc_dir.parent
        if not env_get(env, "VSINSTALLDIR"):
            env_set(env, "VSINSTALLDIR", path_value(vs_root))
        env_set(env, common_tools_var, path_value(vs_root / "Common7" / "Tools"))


def get_vs2013_env(vs2013_vcvarsall: Path | None = None) -> dict[str, str]:
    global VS2013_ENV
    if VS2013_ENV is not None:
        return VS2013_ENV

    vcvarsall = get_vs2013_vcvarsall(vs2013_vcvarsall)
    VS2013_ENV = capture_vcvarsall_env(
        vcvarsall,
        "VS2013",
        ["INCLUDE", "LIB", "PATH", "VCINSTALLDIR"]
    )
    ensure_vs_common_tools_env("12.0", vcvarsall, VS2013_ENV)
    if not env_get(VS2013_ENV, "VS120COMNTOOLS"):
        log_exc("VS2013 environment did not provide enough information to synthesize VS120COMNTOOLS.")
    return VS2013_ENV


def get_vs2015_env() -> dict[str, str]:
    global VS2015_ENV
    if VS2015_ENV is not None:
        return VS2015_ENV

    VS2015_ENV = capture_vcvarsall_env(
        get_vs2015_vcvarsall(),
        "VS2015",
        ["INCLUDE", "LIB", "PATH", "VS140COMNTOOLS", "VCINSTALLDIR"]
    )
    return VS2015_ENV


def set_github_pat(token: str | None) -> None:
    global GITHUB_PAT
    GITHUB_PAT = token.strip() if token is not None and len(token.strip()) > 0 else None


def require_github_pat() -> str:
    if GITHUB_PAT is not None:
        return GITHUB_PAT
    log_exc("GitHub API release asset downloads require --git-pat.")


def github_release_asset_request(url: str) -> urllib.request.Request:
    headers = {
        "Accept": "application/octet-stream",
        "User-Agent": "curl",
    }
    if urllib.parse.urlparse(url).netloc.casefold() == "api.github.com":
        headers["Authorization"] = f"token {require_github_pat()}"
    return urllib.request.Request(url, headers=headers)


def extract_zip(zip_path: Path, destination: Path, ignore_bad_crc: bool = False):
    with zipfile.ZipFile(zip_path) as zip_:
        for member in zip_.infolist():
            member_name = member.filename.replace("\\", "/")
            try:
                zip_.extract(member, destination)
            except zipfile.BadZipFile as ex:
                if ignore_bad_crc and "Bad CRC-32" in str(ex):
                    logging.warning(f"Ignoring bad CRC while extracting {zip_path.name}; keeping extracted bytes for {member_name}")
                    continue
                raise


def _safe_archive_member_name(name: str) -> str:
    raw_name = name.replace("\\", "/")
    path = PurePosixPath(raw_name)

    if "\0" in raw_name:
        raise ValueError(f"Archive member contains a null byte: {name!r}")
    if path.is_absolute():
        raise ValueError(f"Archive member is absolute: {name!r}")
    if len(raw_name) >= 2 and raw_name[1] == ":":
        raise ValueError(f"Archive member has a drive prefix: {name!r}")

    parts = [part for part in raw_name.split("/") if part not in ("", ".")]
    if not parts or any(part == ".." for part in parts):
        raise ValueError(f"Archive member is not a safe relative path: {name!r}")

    return "/".join(parts)


def _apply_tar_metadata(member: tarfile.TarInfo, target: Path) -> None:
    try:
        if not member.issym():
            target.chmod(member.mode)
    except OSError:
        pass

    try:
        if not member.issym():
            os.utime(target, (member.mtime, member.mtime))
    except OSError:
        pass


def _extract_progress_message(
    archive_path: Path,
    file_count: int,
    directory_count: int,
    extracted_bytes: int,
    current_member: str | None,
) -> str:
    size_gib = extracted_bytes / (1024 ** 3)
    suffix = f"; current: {current_member}" if current_member else ""
    return (
        f"Extracting {archive_path.name}: {file_count} file(s), "
        f"{directory_count} directory entry/entries, {size_gib:.2f} GiB written{suffix}"
    )


def _log_extract_progress(
    archive_path: Path,
    file_count: int,
    directory_count: int,
    extracted_bytes: int,
    current_member: str | None,
    last_log_time: float,
    *,
    force: bool = False,
) -> float:
    now = time.monotonic()
    if force or now - last_log_time >= TAR_ZST_EXTRACT_LOG_INTERVAL_SECONDS:
        logging.info(
            _extract_progress_message(
                archive_path,
                file_count,
                directory_count,
                extracted_bytes,
                current_member,
            )
        )
        return now
    return last_log_time


def extract_tar_zst(archive_path: Path, destination: Path):
    logging.info(f"Extracting {archive_path} to {destination}...")
    file_count = 0
    directory_count = 0
    extracted_bytes = 0
    last_log_time = time.monotonic()

    with zstd.open(archive_path, "rb") as compressed:
        with tarfile.open(fileobj=compressed, mode="r|") as tar_:
            for member in tar_:
                try:
                    member_name = _safe_archive_member_name(member.name)
                    if member.islnk() or member.issym():
                        _safe_archive_member_name(member.linkname)
                except ValueError as ex:
                    log_exc(f"Refusing to extract unsafe archive member from {archive_path}: {ex}")

                target = destination / member_name
                if member.isdir():
                    target.mkdir(parents=True, exist_ok=True)
                    _apply_tar_metadata(member, target)
                    directory_count += 1
                    last_log_time = _log_extract_progress(
                        archive_path, file_count, directory_count, extracted_bytes, member_name, last_log_time
                    )
                    continue

                if member.isfile():
                    source = tar_.extractfile(member)
                    if source is None:
                        log_exc(f"Failed to open archive member {member.name} from {archive_path}")

                    target.parent.mkdir(parents=True, exist_ok=True)
                    with source, open(target, "wb") as output_file:
                        while True:
                            chunk = source.read(TAR_ZST_COPY_BUFFER_SIZE)
                            if len(chunk) == 0:
                                break
                            output_file.write(chunk)
                            extracted_bytes += len(chunk)
                            last_log_time = _log_extract_progress(
                                archive_path, file_count, directory_count, extracted_bytes, member_name, last_log_time
                            )

                    _apply_tar_metadata(member, target)
                    file_count += 1
                    continue

                target.parent.mkdir(parents=True, exist_ok=True)
                tar_.extract(member, destination)
                _apply_tar_metadata(member, target)

    _log_extract_progress(archive_path, file_count, directory_count, extracted_bytes, None, last_log_time, force=True)


def archive_kind(archive_path: Path) -> str:
    name = archive_path.name.lower()
    if name.endswith(".tar.zst"):
        return "tar.zst"
    if archive_path.suffix.lower() == ".zip":
        return "zip"
    log_exc(f"Unsupported Unreal dependency archive type: {archive_path}")


def is_archive_file(archive_path: Path, kind: str | None = None) -> bool:
    kind = archive_kind(archive_path) if kind is None else kind
    if kind == "zip":
        return zipfile.is_zipfile(archive_path)
    if kind == "tar.zst":
        try:
            with zstd.open(archive_path, "rb") as compressed:
                with tarfile.open(fileobj=compressed, mode="r|") as tar_:
                    return tar_.next() is not None
        except (OSError, EOFError, tarfile.TarError, zstd.ZstdError):
            return False
    log_exc(f"Unsupported Unreal dependency archive type: {archive_path}")


def extract_archive(archive_path: Path, destination: Path, ignore_bad_crc: bool = False):
    kind = archive_kind(archive_path)
    if kind == "zip":
        extract_zip(archive_path, destination, ignore_bad_crc)
        return
    if kind == "tar.zst":
        extract_tar_zst(archive_path, destination)
        return
    log_exc(f"Unsupported Unreal dependency archive type: {archive_path}")


def get_mega_get_path() -> Path:
    if mega_get := shutil.which("mega-get"):
        return Path(mega_get)

    candidates = [
        path_from_value(os.environ.get("UEMETA_MEGA_GET")),
        Path(os.environ["LOCALAPPDATA"]) / "MEGAcmd" / "mega-get.bat" if os.environ.get("LOCALAPPDATA") else None,
        Path(os.environ["ProgramFiles"]) / "MEGAcmd" / "mega-get.bat" if os.environ.get("ProgramFiles") else None,
        Path(os.environ["ProgramFiles(x86)"]) / "MEGAcmd" / "mega-get.bat" if os.environ.get("ProgramFiles(x86)") else None,
    ]
    for candidate in candidates:
        if candidate is not None and candidate.exists():
            return candidate

    log_exc("Failed to find mega-get. Install MEGAcmd or set UEMETA_MEGA_GET to mega-get.bat.")


def download_mega_public_file(url: str, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    download_dir = destination.with_name(destination.name + ".mega")
    if download_dir.exists():
        shutil.rmtree(download_dir)
    download_dir.mkdir(parents=True)

    try:
        execute(
            [get_mega_get_path(), url, download_dir],
            success_msg=f"Downloaded MEGA file to {destination}.",
            fail_msg=f"Failed to download MEGA file {url}!",
            output=ExecuteOutputOptions.FILE | ExecuteOutputOptions.STDOUT,
        )
        downloaded_files = [path for path in download_dir.rglob("*") if path.is_file()]
        if len(downloaded_files) != 1:
            log_exc(f"Expected MEGAcmd to download one file from {url}, found {len(downloaded_files)}.")
        downloaded_files[0].replace(destination)
    finally:
        shutil.rmtree(download_dir, ignore_errors=True)


def dependency_archive_cache_path(cache_root: Path, filename: str) -> Path:
    return cache_root / filename


def dependency_zip_cache_path(cache_root: Path, part: int, total_parts: int) -> Path:
    return dependency_archive_cache_path(cache_root, f"Required_{part}of{total_parts}.zip")


def ensure_archive_cached(
    url: str,
    archive_path: Path,
    *,
    label: str = "Unreal dependency archive",
    request_factory: Callable[[str], urllib.request.Request | str] | None = None,
    download_factory: Callable[[str, Path], None] | None = None,
    http_error_hint: str = "",
):
    kind = archive_kind(archive_path)
    if archive_path.exists():
        if is_archive_file(archive_path, kind):
            logging.info(f"Using cached {label}: {archive_path}")
            return
        logging.warning(f"Cached {label} is invalid and will be replaced: {archive_path}")
        archive_path.unlink()

    archive_path.parent.mkdir(parents=True, exist_ok=True)
    download_path = archive_path.with_name(archive_path.name + ".download")
    try:
        if download_factory is not None:
            download_factory(url, download_path)
        else:
            request: urllib.request.Request | str = request_factory(url) if request_factory is not None else url
            with urllib.request.urlopen(request) as response:
                with open(download_path, "wb") as download_file:
                    shutil.copyfileobj(response, download_file)

        logging.info(f"Validating downloaded {label}: {download_path}")
        if not is_archive_file(download_path, kind):
            log_exc(f"Downloaded {label} is not a valid {kind} file: {url}")

        download_path.replace(archive_path)
        logging.info(f"Cached {label}: {archive_path}")
    except urllib.error.HTTPError as ex:
        hint = f" {http_error_hint}" if http_error_hint else ""
        log_exc(f"Failed to download {label} {url}: HTTP {ex.code}.{hint}")
    except urllib.error.URLError as ex:
        log_exc(f"Failed to download {label} {url}: {ex.reason}")
    finally:
        if download_path.exists():
            download_path.unlink(missing_ok=True)


def ensure_release_asset_cached(url: str, zip_path: Path):
    ensure_archive_cached(
        url,
        zip_path,
        label="Unreal dependency zip",
        request_factory=github_release_asset_request,
        http_error_hint="Pass --git-pat with access to EpicGames/UnrealEngine.",
    )


def download_and_extract_archive(
    url: str,
    archive_path: Path,
    destination: Path,
    ignore_bad_crc: bool = False,
    *,
    label: str = "Unreal dependency archive",
    request_factory: Callable[[str], urllib.request.Request | str] | None = None,
    download_factory: Callable[[str, Path], None] | None = None,
    http_error_hint: str = "",
):
    try:
        ensure_archive_cached(
            url,
            archive_path,
            label=label,
            request_factory=request_factory,
            download_factory=download_factory,
            http_error_hint=http_error_hint,
        )
        extract_archive(archive_path, destination, ignore_bad_crc)
    except zipfile.BadZipFile as ex:
        log_exc(f"Failed to extract Unreal dependency zip {archive_path}: {ex}")
    except (OSError, EOFError, tarfile.TarError, zstd.ZstdError) as ex:
        log_exc(f"Failed to extract Unreal dependency archive {archive_path}: {ex}")


def download_and_extract_release_asset(url: str, zip_path: Path, destination: Path, ignore_bad_crc: bool = False):
    download_and_extract_archive(
        url,
        zip_path,
        destination,
        ignore_bad_crc,
        label="Unreal dependency zip",
        request_factory=github_release_asset_request,
        http_error_hint="Pass --git-pat with access to EpicGames/UnrealEngine.",
    )
