import logging
import subprocess
from enum import Flag, auto
from os import PathLike
from typing import NoReturn, Optional
from pathlib import Path

from tqdm.contrib.logging import logging_redirect_tqdm

logging.basicConfig(level=logging.INFO, format='[%(levelname)s] [%(asctime)s] %(message)s',
                    handlers=[logging.StreamHandler(), logging.FileHandler(Path().cwd() / "intermediate" / "coordinator.log", mode='w')],
                    force=True)

def with_tqdm_logging(func):
    with logging_redirect_tqdm():
        return func()

def log_exc(msg: str, exc: type[Exception] = Exception) -> NoReturn:
    logging.error(msg)
    raise exc(msg)

def exec_proc(argv: list[str | PathLike[str]] | str | PathLike[str], success_msg: Optional[str] = None, fail_msg: Optional[str] = None,
              expected_ret = 0, log_output = True, cwd: str | PathLike[str] | None = None, soft_fail = False) -> tuple[int, str]:
    out = ""
    with subprocess.Popen(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1, cwd=cwd) as proc:
        if proc.stdout:
            for line in proc.stdout:
                if log_output:
                    logging.info(line.strip())
                out += line

    return_code = proc.wait()
    proc.kill()
    if return_code == expected_ret:
        if log_output and (success_msg is not None):
            logging.info(success_msg)
        return return_code, out
    elif soft_fail:
        return return_code, out
    else:
        log_exc(fail_msg if fail_msg is not None else "Unknown error!")
#todo add log to file at end, stream output option with ability to bypass logger

class ExecuteOutputOptions(Flag):
    # No output from subprocess.
    SILENT = 0

    # Logs each line with logger (with timestamps and console/file output).
    # LOGGER | (STDOUT or FILE) is the same as LOGGER (STDOUT/FILE is ignored when combined with LOGGER)
    LOGGER = auto()

    # Logs each line to stdout.
    # Does not use the logger (so no file output or timestamps on its own)
    STDOUT = auto()

    # Logs each line to the file associated with the current logger.
    # Does not use the logger (so no console output or timestamps on its own)
    # When | STREAM_STDOUT, the file won't be updated until the process exits
    FILE = auto()

def execute(argv: list[str | PathLike[str]] | str | PathLike[str], *,
            success_msg: Optional[str] = None,
            fail_msg: Optional[str] = None,
            expected_ret = 0,
            cwd: Optional[str | PathLike[str]] = None,
            except_on_error = True,
            output = ExecuteOutputOptions.LOGGER):
    pass