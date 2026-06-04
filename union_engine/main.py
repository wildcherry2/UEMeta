from Config import GLOBAL_CONFIG
from multiprocessing import Pool
from Group import VersionGroup, group_by_name
from Union import union

def main():
    GLOBAL_CONFIG.initialize()
    groups = [VersionGroup(_) for _ in GLOBAL_CONFIG.get_json_dirs()]
    if len(groups) == 0:
        return 0

    decls = group_by_name(groups)

    with Pool() as pool:
        while(len(groups) > 1):
            pass
    return 0

if __name__ == '__main__':
    main()