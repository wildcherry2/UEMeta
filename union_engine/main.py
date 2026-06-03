from Config import GLOBAL_CONFIG
from multiprocessing import Pool
from FileGroup import FileGroup
from Union import union

def main():
    GLOBAL_CONFIG.initialize()
    groups = [FileGroup(_) for _ in GLOBAL_CONFIG.get_json_dirs()]
    if len(groups) == 0:
        return 0

    with Pool() as pool:
        while(len(groups) > 1):
            odd_item = None
            if len(groups) % 2 != 0:
                odd_item = groups.pop()

            iterator = iter(groups)
            pairs: list[tuple[FileGroup, FileGroup]] = list(zip(iterator, iterator)) # type: ignore
            data = pool.starmap(union, pairs)
    return 0

if __name__ == '__main__':
    main()