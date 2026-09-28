import logging
from typing import NoReturn

def log_exc(msg: str, exc: type[Exception] = Exception) -> NoReturn:
    logging.error(msg)
    raise exc(msg)