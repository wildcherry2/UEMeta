from configparser import UNNAMED_SECTION, ConfigParser, ExtendedInterpolation, SectionProxy
from dataclasses import dataclass
from enum import Enum
from pathlib import Path

class Mode(Enum):
    FULL = "full"
    PARSER_ONLY = "parser_only"
    MERGER_ONLY = "merger_only"
    UPG_ONLY = "upg_only"
    GENERATOR_ONLY = "generator_only"
    PARSER_REPL = "parser_repl"
    FROM_UPG = "from_upg"
    FROM_PARSER = "from_parser"
    FROM_MERGER = "from_merger"
    FROM_GENERATOR = "from_generator"


class ParserFormat(Enum):
    json = "json"
    binary = "binary"


class ParserLogLevel(Enum):
    debug = "debug"
    info = "info"
    error = "error"
    warn = "warn"
    trace = "trace"
    disabled = "disabled"


@dataclass
class UnnamedSection:
    Mode: Mode
    PathToParser: str


@dataclass
class ParserRepl:
    Version: str
    OutDir: str | None = None
    ClangArgs: str | None = None
    Format: ParserFormat | None = None
    LogLevel: ParserLogLevel | None = None


@dataclass
class MergerOnly:
    InputDir: str
    OutputDir: str


@dataclass
class ParserOnly:
    Version: str
    PreferClang: bool | None = None
    PreferFullNameInFileName: bool | None = None
    Sync: bool | None = None
    EnableUnrealExtensions: bool | None = None
    Log: str | None = None
    FileDelimiter: str | None = None
    Output: str | None = None
    Format: ParserFormat | None = None
    LogLevel: ParserLogLevel | None = None
    # One input is required when running the parser; AstFile takes precedence.
    CompileCommands: str | None = None
    StripCommands: str | None = None
    AdditionalClangArgs: str | None = None
    AstFile: str | None = None


@dataclass
class FromParser:
    Versions: list[str]
    AstFiles: list[str]
    Sync: bool | None = None
    EnableUnrealExtensions: bool | None = None


@dataclass
class FromMerger:
    InputDir: str


@dataclass
class Config:
    UNNAMED_SECTION: UnnamedSection
    parser_repl: ParserRepl
    merger_only: MergerOnly
    parser_only: ParserOnly
    from_parser: FromParser
    from_merger: FromMerger


def __get_optional_string(section: SectionProxy, option: str) -> str | None:
    return section.get(option) or None


def __get_enum[E: Enum](section: SectionProxy, option: str, enum_type: type[E]) -> E | None:
    value = __get_optional_string(section, option)
    return enum_type(value) if value else None


def __get_boolean(section: SectionProxy, option: str) -> bool | None:
    return section.getboolean(option) if __get_optional_string(section, option) is not None else None


def __get_list(section: SectionProxy, option: str) -> list[str]:
    return [item.strip() for item in section[option].split(",") if item.strip()]


def __parse_config(ini_path: Path) -> Config:
    """Load sections into dataclasses, retaining INI names and interpolation.

    Blank or missing optional parser settings become None so callers can omit
    those CLI arguments and use the defaults in Cli.cpp. Explicit False flags
    are preserved. Versions/AstFiles are comma-separated lists (empty when blank).
    """
    config = ConfigParser(interpolation=ExtendedInterpolation(), allow_unnamed_section=True)
    with ini_path.open(encoding="utf-8") as ini_file:
        config.read_file(ini_file)

    unnamed = config[UNNAMED_SECTION]
    parser_repl = config["parser_repl"]
    merger_only = config["merger_only"]
    parser_only = config["parser_only"]
    from_parser = config["from_parser"]
    from_merger = config["from_merger"]

    return Config(
        UNNAMED_SECTION=UnnamedSection(
            Mode=Mode(unnamed["Mode"]),
            PathToParser=unnamed["PathToParser"],
        ),
        parser_repl=ParserRepl(
            Version=parser_repl["Version"],
            OutDir=__get_optional_string(parser_repl, "OutDir"),
            ClangArgs=__get_optional_string(parser_repl, "ClangArgs"),
            Format=__get_enum(parser_repl, "Format", ParserFormat),
            LogLevel=__get_enum(parser_repl, "LogLevel", ParserLogLevel),
        ),
        merger_only=MergerOnly(
            InputDir=merger_only["InputDir"],
            OutputDir=merger_only["OutputDir"],
        ),
        parser_only=ParserOnly(
            Version=parser_only["Version"],
            PreferClang=__get_boolean(parser_only, "PreferClang"),
            PreferFullNameInFileName=__get_boolean(parser_only, "PreferFullNameInFileName"),
            Sync=__get_boolean(parser_only, "Sync"),
            EnableUnrealExtensions=__get_boolean(parser_only, "EnableUnrealExtensions"),
            Log=__get_optional_string(parser_only, "Log"),
            FileDelimiter=__get_optional_string(parser_only, "FileDelimiter"),
            Output=__get_optional_string(parser_only, "Output"),
            Format=__get_enum(parser_only, "Format", ParserFormat),
            LogLevel=__get_enum(parser_only, "LogLevel", ParserLogLevel),
            CompileCommands=__get_optional_string(parser_only, "CompileCommands"),
            StripCommands=__get_optional_string(parser_only, "StripCommands"),
            AdditionalClangArgs=__get_optional_string(parser_only, "AdditionalClangArgs"),
            AstFile=__get_optional_string(parser_only, "AstFile"),
        ),
        from_parser=FromParser(
            Versions=__get_list(from_parser, "Versions"),
            AstFiles=__get_list(from_parser, "AstFiles"),
            Sync=__get_boolean(from_parser, "Sync"),
            EnableUnrealExtensions=__get_boolean(from_parser, "EnableUnrealExtensions"),
        ),
        from_merger=FromMerger(InputDir=from_merger["InputDir"]),
    )


CONFIG: Config = __parse_config(Path(__file__).resolve().parent.parent / "config.ini")
