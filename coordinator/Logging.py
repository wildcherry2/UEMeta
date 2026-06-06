import logging
from tqdm.contrib.logging import logging_redirect_tqdm

logging.basicConfig(level=logging.INFO, format='[%(levelname)s] [%(asctime)s] %(message)s',
                    handlers=[logging.StreamHandler(), logging.FileHandler("coordinator.log")],
                    force=True)

def with_tqdm_logging(func):
    with logging_redirect_tqdm():
        return func()