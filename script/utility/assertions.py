from os import PathLike
from pathlib import Path
from utility.log_util import log_exc

def assert_file_exists(path_str: str | PathLike[str] | Path) -> Path:
    as_path = Path(path_str)
    if as_path.exists():
        return as_path
    log_exc(f"{as_path} does not exist!", ValueError)