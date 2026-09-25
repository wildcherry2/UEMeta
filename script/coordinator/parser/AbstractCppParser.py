import json
from os import PathLike
from pathlib import Path
from abc import abstractmethod

from GlobalUtil import ExecuteOutputOptions, execute, log_exc
from parser.AbstractParser import AbstractParser
from unreal.Util import CanonicalVersion


class AbstractCppParser(AbstractParser):
    @abstractmethod
    def make_compile_commands(self, branch: str) -> Path:
        pass

    @abstractmethod
    def get_target_cpp(self) -> Path:
        pass

    def parse(self, branch: str):
        canon = CanonicalVersion(branch)
        cc = self.make_compile_commands(branch)
        target_cpp = self.get_target_cpp()
        filtered_cc = AbstractCppParser.__filter_compile_commands(cc, target_cpp)
        parser_working_dir = self.parser_out / canon.version
        parser_working_dir.mkdir(exist_ok=True, parents=True)
        execute(AbstractCppParser.__generate_parse_command(self.parser_path, filtered_cc, parser_working_dir,
                                        self.parser_additional_commands),
                        success_msg=f"Parsing complete for branch {canon.version}",
                        fail_msg=f"Parsing failed for branch {canon.version}", cwd=self.parser_path.parent,
                        output=ExecuteOutputOptions.FILE | ExecuteOutputOptions.STDOUT)

    @staticmethod
    def __filter_compile_commands(cc: Path, target_cpp: Path) -> str:
        try:
            with open(cc, "r", encoding="utf-8") as compile_commands_file:
                compile_commands = json.load(compile_commands_file)
        except (OSError, json.JSONDecodeError) as e:
            log_exc(f"Failed to load compilation database {cc}: {e}", ValueError)

        if not isinstance(compile_commands, list):
            log_exc(f"Compilation database {cc} does not contain a JSON array!", ValueError)

        resolved_target = target_cpp.resolve()
        matches = []
        for command in compile_commands:
            if not isinstance(command, dict) or not isinstance(command.get("file"), str):
                continue

            command_file = Path(command["file"])
            if not command_file.is_absolute():
                directory = command.get("directory")
                command_directory = Path(directory) if isinstance(directory, str) else cc.parent
                if not command_directory.is_absolute():
                    command_directory = cc.parent / command_directory
                command_file = command_directory / command_file

            if command_file.resolve() == resolved_target:
                matches.append(command)

        if len(matches) == 0:
            log_exc(f"Failed to find {resolved_target} in compilation database {cc}!", ValueError)

        return json.dumps(matches, separators=(",", ":"))

    @staticmethod
    def __generate_parse_command(parser_path: Path, compile_commands: str, out: Path,
                                 addl_cmds: list[str]) -> list[str | PathLike[str]]:
        return [parser_path, "--compile-commands", compile_commands, "--output", out, *addl_cmds]
