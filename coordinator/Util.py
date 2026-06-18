import os
import logging
import subprocess
import sys
from enum import Flag, auto
from functools import partial
from os import PathLike
from typing import NoReturn, Optional, cast

def log_exc(msg: str, exc: type[Exception] = Exception) -> NoReturn:
    logging.error(msg)
    raise exc(msg)

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
    FILE = auto()

class ExecuteException(Exception):
    def __init__(self, return_code: int, expected_return_code: int, stdout: str, *args):
        super().__init__(*args)
        self.return_code = return_code
        self.expected_return_code = expected_return_code
        self.stdout = stdout

def _merge_env(addl_env: dict[str, str]) -> dict[str, str]:
    env = os.environ.copy()
    if os.name != "nt":
        return env | addl_env

    env_keys = {key.upper(): key for key in env}
    for key, value in addl_env.items():
        existing_key = env_keys.get(key.upper())
        if existing_key is not None and existing_key != key:
            env.pop(existing_key, None)
        env[key] = value
        env_keys[key.upper()] = key
    return env

def execute(argv: list[str | PathLike[str]] | str | PathLike[str] | list[str], *,
            success_msg: Optional[str] = None,
            fail_msg: Optional[str] = None,
            expected_ret = 0,
            cwd: Optional[str | PathLike[str]] = None,
            raise_on_error: Optional[type[ExecuteException]] = ExecuteException,
            log_on_error: bool = True,
            output = ExecuteOutputOptions.LOGGER,
            addl_env: Optional[dict[str, str]] = None) -> tuple[int, str]:

    out = ""
    is_silent = output == ExecuteOutputOptions.SILENT
    use_logger = output & ExecuteOutputOptions.LOGGER
    use_stdout = (not use_logger) and output & ExecuteOutputOptions.STDOUT
    use_file = (not use_logger) and output & ExecuteOutputOptions.FILE

    if addl_env is not None and len(addl_env) > 0:
        addl_env = _merge_env(addl_env)

    with subprocess.Popen(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1, cwd=cwd, env=addl_env) as proc:
        if proc.stdout:
            for line in proc.stdout:
                out += line
                if not is_silent:
                    if use_logger:
                        stripped = line.strip()
                        logging.info(stripped)
                    if use_stdout:
                        sys.stdout.write(line)
                    if use_file:
                        all_files = [handler for handler in logging.getLogger().handlers if isinstance(handler, logging.FileHandler)]
                        for file in all_files:
                            if file.stream is not None:
                                file.stream.write(line)
                                file.stream.flush()

    return_code = proc.wait()

    if return_code == expected_ret:
        if success_msg is not None:
            logging.info(success_msg)
    elif raise_on_error is not None:
        msg = fail_msg + f" (process returned {return_code})" if fail_msg is not None else f"Error! (process returned {return_code})!"
        if log_on_error:
            log_exc(msg, cast(type[Exception], partial(raise_on_error, return_code, expected_ret, out)))
        else:
            raise raise_on_error(return_code, expected_ret, out)

    return return_code, out
