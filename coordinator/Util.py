import logging
import subprocess
from os import PathLike
from typing import NoReturn

from tqdm.contrib.logging import logging_redirect_tqdm

logging.basicConfig(level=logging.INFO, format='[%(levelname)s] [%(asctime)s] %(message)s',
                    handlers=[logging.StreamHandler(), logging.FileHandler("coordinator.log", mode='w')],
                    force=True)

def with_tqdm_logging(func):
    with logging_redirect_tqdm():
        return func()

def log_exc(msg: str, exc: type[Exception] = Exception) -> NoReturn:
    logging.error(msg)
    raise exc(msg)

def exec_proc(argv: list[str | PathLike[str]] | str | PathLike[str], success_msg: str, fail_msg: str,
              expected_ret = 0, log_output = True, cwd: str | PathLike[str] | None = None) -> str:
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
        logging.info(success_msg)
        return out
    else:
        log_exc(fail_msg)
