import configparser
import os
from dataclasses import dataclass
from pathlib import Path
from typing import overload

from GlobalUtil import assert_file_exists, log_exc


@dataclass(frozen=True)
class CoordinatorConfig:
    config_path: Path
    git_pat: str | None
    mode: str
    intermediate_directory: Path
    repo_url: str
    branches: list[str]
    parser_path: Path
    parser_additional_commands: list[str]
    public_dependency_module_names: list[str]
    headers: list[str]
    ubt_platform: str
    ubt_config: str
    vs2013_vcvarsall: Path | None
    use_existing_project: bool

def load_config(config_path: str | Path, git_pat: str | None = None) -> CoordinatorConfig:
    resolved_config_path = Path(config_path).resolve()
    parser = configparser.ConfigParser(interpolation=None)
    try:
        with open(resolved_config_path, "r", encoding="utf-8") as config_file:
            parser.read_file(config_file)
    except OSError as e:
        log_exc(f"Failed to read config file {resolved_config_path}: {e}")
    except configparser.Error as e:
        log_exc(f"Failed to parse config file {resolved_config_path}: {e}")

    base_dir = resolved_config_path.parent
    parser_path = _resolve_existing_path(base_dir, _get_required(parser, "Parser", "parser_path"))
    if parser_path.is_dir():
        executable_name = "parser.exe" if _is_windows_parser_host() else "parser"
        parser_path = assert_file_exists(parser_path / executable_name)

    return CoordinatorConfig(
        config_path=resolved_config_path,
        git_pat=_empty_to_none(git_pat),
        mode=_get(parser, "General", "mode", "Unreal"),
        intermediate_directory=_resolve_path(base_dir, _get_required(parser, "General", "intermediate_directory")),
        repo_url=_get_required(parser, "Git Target", "url"),
        branches=_get_list(parser, "Git Target", "branches", required=True),
        parser_path=parser_path,
        parser_additional_commands=_get_list(parser, "Parser", "parser_additional_commands"),
        public_dependency_module_names=_get_list(
            parser,
            "Unreal Parser",
            "public_dependency_module_names",
            fallback=["Core", "CoreUObject", "Engine"],
        ),
        headers=_get_list(parser, "Unreal Parser", "headers", required=True),
        ubt_platform=_get(parser, "Unreal Parser", "platform", "Win64"),
        ubt_config=_get(parser, "Unreal Parser", "config", "Shipping"),
        vs2013_vcvarsall=_get_optional_path(parser, base_dir, "Unreal Parser", "vs2013_vcvarsall"),
        use_existing_project=parser.getboolean("Unreal Parser", "use_existing_project", fallback=False)
    )


def _get_required(parser: configparser.ConfigParser, section: str, option: str) -> str:
    value = _get(parser, section, option, None)
    if value is None or len(value) == 0:
        log_exc(f"Missing required config value [{section}] {option}")
    return value


@overload
def _get(parser: configparser.ConfigParser, section: str, option: str, fallback: str) -> str:
    ...


@overload
def _get(parser: configparser.ConfigParser, section: str, option: str, fallback: None) -> str | None:
    ...


def _get(parser: configparser.ConfigParser, section: str, option: str, fallback: str | None) -> str | None:
    if not parser.has_section(section):
        if fallback is not None:
            return fallback
        log_exc(f"Missing required config section [{section}]")

    if not parser.has_option(section, option):
        return fallback

    return _strip_optional_quotes(parser.get(section, option))


def _get_list(
    parser: configparser.ConfigParser,
    section: str,
    option: str,
    *,
    fallback: list[str] | None = None,
    required: bool = False,
) -> list[str]:
    raw_value = _get(parser, section, option, None)
    values = _split_config_list(raw_value) if raw_value is not None else list(fallback or [])
    if required and len(values) == 0:
        log_exc(f"Missing required config list [{section}] {option}")
    return values


def _split_config_list(value: str) -> list[str]:
    lines = [_strip_optional_quotes(line.strip()) for line in value.splitlines()]
    lines = [line for line in lines if len(line) > 0]
    if len(lines) != 1:
        return lines
    return [item for item in lines[0].split() if len(item) > 0]


def _get_optional_path(
    parser: configparser.ConfigParser,
    base_dir: Path,
    section: str,
    option: str,
) -> Path | None:
    value = _get(parser, section, option, None)
    return _resolve_path(base_dir, value) if value is not None and len(value) > 0 else None


def _resolve_existing_path(base_dir: Path, value: str) -> Path:
    return assert_file_exists(_resolve_path(base_dir, value))


def _resolve_path(base_dir: Path, value: str) -> Path:
    path = Path(_strip_optional_quotes(value))
    if not path.is_absolute():
        path = base_dir / path
    return path.resolve()


def _strip_optional_quotes(value: str) -> str:
    value = value.strip()
    if len(value) >= 2 and value[0] == value[-1] and value[0] in ("'", '"'):
        return value[1:-1]
    return value


def _empty_to_none(value: str | None) -> str | None:
    if value is None:
        return None
    value = value.strip()
    return value if len(value) > 0 else None


def _is_windows_parser_host() -> bool:
    return os.name == "nt"
