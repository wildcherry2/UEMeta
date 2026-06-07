import logging
import subprocess
from os import PathLike
from typing import NoReturn

from tqdm.contrib.logging import logging_redirect_tqdm

logging.basicConfig(level=logging.INFO, format='[%(levelname)s] [%(asctime)s] %(message)s',
                    handlers=[logging.StreamHandler(), logging.FileHandler("coordinator.log")],
                    force=True)

def with_tqdm_logging(func):
    with logging_redirect_tqdm():
        return func()

def log_exc(msg: str, exc: type[Exception] = Exception) -> NoReturn:
    logging.error(msg)
    raise exc(msg)

def exec_proc(argv: list[str | PathLike[str]] | str | PathLike[str], success_msg: str, fail_msg: str, expected_ret = 0, out: str | None = None):
    with subprocess.Popen(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1) as proc:
        if proc.stdout:
            for line in proc.stdout:
                if out is None:
                    logging.info(line.strip())
                else:
                    out += line

    return_code = proc.wait()
    if return_code == expected_ret:
        logging.info(success_msg)
    else:
        log_exc(fail_msg)