from pathlib import Path
from typing import Literal

from .execute import execute


def invoke_parser_repl(parser: Path, version: str, *,
                       prefer_full_name_in_file_name = False,
                       sync = False,
                       log: Path | None = None,
                       clang_args: list[str] | None = None,
                       output: Path | None = None,
                       format: Literal["json", "binary"] | None = None,
                       log_level: Literal["debug", "info", "warn", "error", "trace", "disabled"] | None = None):
    args = [str(parser), "repl", "--version", version]
    if prefer_full_name_in_file_name:
        args.append("--prefer-full-name-in-file-name")
    if sync:
        args.append("--sync")
    for option, value in (("--log-level", log_level), ("--log", log),
                          ("--output", output), ("--format", format)):
        if value is not None:
            args.extend([option, str(value)])
    if clang_args:
        # Bind compiler flags with '=' so a leading '-' stays part of the value.
        args.append("--clang-args=" + ",".join(clang_args))
    execute(args)


def invoke_parser_cc(parser: Path, version: str, compile_commands: str | Path, *,
                     prefer_full_name_in_file_name=False,
                     sync=False,
                     log: Path | None = None,
                     prefer_clang = False,
                     enable_unreal_extensions = False,
                     file_delimiter: str | None = None,
                     output: Path | None = None,
                     log_level: Literal["debug", "info", "warn", "error", "trace", "disabled"] | None = None,
                     format: Literal["json", "binary"] | None = None,
                     strip_commands: list[str] | None = None,
                     additional_clang_args: list[str] | None = None):
    args = [str(parser), "parse", "cc",
            str(compile_commands.resolve() if isinstance(compile_commands, Path) else compile_commands),
            "--version", version]
    if prefer_full_name_in_file_name:
        args.append("--prefer-full-name-in-file-name")
    if sync:
        args.append("--sync")
    if prefer_clang:
        args.append("--prefer-clang")
    if enable_unreal_extensions:
        args.append("--enable-unreal-extensions")
    for option, value in (("--log-level", log_level), ("--log", log),
                          ("--output", output), ("--format", format),
                          ("--file-delimiter", file_delimiter)):
        if value is not None:
            args.extend([option, str(value)])
    for option, values in (("--strip-commands", strip_commands),
                           ("--additional-clang-args", additional_clang_args)):
        if values:
            # Bind compiler flags with '=' so a leading '-' stays part of the value.
            args.append(option + "=" + ",".join(values))
    execute(args)


def invoke_parser_cache(parser: Path, version: str, ast_file: Path, *,
                        prefer_full_name_in_file_name=False,
                        sync=False,
                        log: Path | None = None,
                        prefer_clang = False,
                        enable_unreal_extensions = True,
                        file_delimiter: str | None = None,
                        output: Path | None = None,
                        log_level: Literal["debug", "info", "warn", "error", "trace", "disabled"] | None = None,
                        format: Literal["json", "binary"] | None = None):
    args = [str(parser), "parse", "ast", str(ast_file.resolve()), "--version", version]
    if prefer_full_name_in_file_name:
        args.append("--prefer-full-name-in-file-name")
    if sync:
        args.append("--sync")
    if prefer_clang:
        args.append("--prefer-clang")
    if enable_unreal_extensions:
        args.append("--enable-unreal-extensions")
    for option, value in (("--log-level", log_level), ("--log", log),
                          ("--output", output), ("--format", format),
                          ("--file-delimiter", file_delimiter)):
        if value is not None:
            args.extend([option, str(value)])
    execute(args)
