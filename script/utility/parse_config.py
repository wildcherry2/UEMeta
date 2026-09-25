from configparser import ConfigParser, ExtendedInterpolation
from enum import Enum
from pathlib import Path

class Mode(Enum):
    FULL = "full"
    PARSER_ONLY = "parser_only"
    MERGER_ONLY = "merger_only"
    UPG_ONLY = "upg_only"
    PARSER_REPL = "parser_repl"
    FROM_UPG = "from_upg"
    FROM_MERGER = "from_merger"

def parse_config(ini_path: Path):
    config = ConfigParser(interpolation=ExtendedInterpolation(), allow_unnamed_section=True)
    config.read(ini_path)
    # throws if mode isn't valid
    Mode(config["UNNAMED_SECTION"]["mode"])
