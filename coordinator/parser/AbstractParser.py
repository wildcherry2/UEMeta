from GlobalUtil import assert_file_exists
import logging
import sys
from abc import abstractmethod, ABC
from pathlib import Path
from typing import Final, Literal, cast

from CoordinatorConfig import CoordinatorConfig
from .Git import Git
from GlobalUtil import log_exc

class AbstractParser(ABC):
    def __init__(self, config: CoordinatorConfig):
        self.config: Final[CoordinatorConfig] = config
        logging.basicConfig(level=logging.INFO, format='[%(levelname)s] [%(asctime)s] %(message)s',
                            handlers=[logging.StreamHandler(),
                                      logging.FileHandler(Path().cwd() / "coordinator.log", mode='w')],
                            force=True)
        self.branches: list[str] = list(dict.fromkeys(config.branches))
        self.repo_url: Final[str] = config.repo_url
        self.intermediate_path: Final[Path] = config.intermediate_directory.resolve()
        self.parser_path: Final[Path] = assert_file_exists(config.parser_path).resolve()
        self.target_repo_path: Final[Path] = self.intermediate_path / "UnrealEngine"
        self.test_project_path: Final[Path] = self.intermediate_path / "MetadataHarness"
        self.parser_out: Final[Path] = self.intermediate_path / "parser_output"
        self.parser_additional_commands: Final[list[str]] = list(config.parser_additional_commands)
        self.platform: Final[Literal["win32", "darwin", "linux"]] = cast(Literal["win32", "darwin", "linux"], sys.platform)
        self.git = Git(self.intermediate_path, self.repo_url)
        try:
            self.intermediate_path.mkdir(exist_ok=True, parents=True)
        except Exception as e:
            log_exc(f"Failed to create intermediate directory \"{self.intermediate_path}\": {e}", ValueError)

        self.__started = False

    def checkout(self, branch: str, prevent_checkout_hooks: bool = False, is_tag: bool = False):
        self.git.checkout(branch, prevent_hooks=prevent_checkout_hooks, force=True, is_tag=is_tag)

    @abstractmethod
    def parse(self, branch: str):
        pass

    def start(self):
        if self.__started:
            log_exc("Failed to start driver: already started!")
        self.__started = True
        logging.info("Starting driver!")
        for branch in self.branches:
            self.checkout(branch)
            self.parse(branch)
