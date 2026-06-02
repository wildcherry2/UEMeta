from Config import GLOBAL_CONFIG
from multiprocessing import Pool
from Union import union

def main():
    # groups = [FileGroup(_) for _ in GLOBAL_CONFIG.get_json_dirs()]
    GLOBAL_CONFIG.initialize()
    dirs = GLOBAL_CONFIG.get_json_dirs()
    if len(dirs) == 0:
        return 0

    with Pool() as pool:
        while(len(dirs) > 1):
            odd_item = None
            if len(dirs) % 2 != 0:
                odd_item = dirs.pop()

            iterator = iter(dirs)
            pairs: list[tuple[str, str]] = list(zip(iterator, iterator)) # type: ignore
            data = pool.starmap(union, pairs)
    return 0

if __name__ == '__main__':
    main()