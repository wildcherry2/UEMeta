from os import PathLike
from GlobalUtil import ExecuteOutputOptions
from GlobalUtil import execute
from pathlib import Path
from abc import abstractmethod
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
        parser_working_dir = self.parser_out / canon.version
        parser_working_dir.mkdir(exist_ok=True, parents=True)
        execute(AbstractCppParser.__generate_parse_command(self.parser_path, target_cpp, cc, parser_working_dir,
                                        self.parser_additional_commands),
                        success_msg=f"Parsing complete for branch {canon.version}",
                        fail_msg=f"Parsing failed for branch {canon.version}", cwd=self.parser_path.parent,
                        output=ExecuteOutputOptions.FILE | ExecuteOutputOptions.STDOUT)

    @staticmethod
    def __generate_parse_command(parser_path: Path, target_cpp: Path, cc: Path, out: Path, addl_cmds: list[str]) -> list[str | PathLike]:
        return [parser_path, "--file", target_cpp, "--compile-commands", cc,
                "--out", out, "--split-strategy", "decl", *addl_cmds]