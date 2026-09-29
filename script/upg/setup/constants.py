import re
from collections import defaultdict
from functools import partial
from pathlib import Path

def __make_local_gitdep_map() -> dict[int, dict[int, dict[int, Path]]]:
    out: dict[int, dict[int, dict[int, Path]]] = defaultdict(partial(defaultdict, dict))
    external_path = Path(__file__).parent / "external"
    if not external_path.exists() or not external_path.is_dir():
        raise Exception(f"external_path for local git dep patches does not exist: {external_path}")
    pattern = re.compile(r"^Commit\.gitdeps\.(?P<version>(?P<major>\d+)\.(?P<minor>\d+)\.(?P<patch>\d+))\.xml$",
               flags=re.MULTILINE)
    for file in external_path.iterdir():
        match = pattern.match(file.name)
        if not match:
            raise Exception(f"unable to parse out version from {file.name}")
        out[int(match.group("version"))][int(match.group("major"))][int(match.group("minor"))] = file
    return out

LOCAL_GIT_DEPS = __make_local_gitdep_map()

CDN_DEPS = {
    4 : {
        6 : "https://mega.nz/file/3nphwapA#wQhpOz03GNXc0zZCIge673O8sNvOb-LDK_W1NnvrUe0",
        7 : "https://mega.nz/file/Kq5kjZxR#ojrVU8sRXrV3KF_pNPLcFXIihg-qm5CMRJOCBTKrd7U",
        8 : "https://mega.nz/file/Xj5mXCYK#UT1QIteq7HgxAHBnOJVLlmVT_kf4xHjEIt5F9plmAVE",
        9 : "https://mega.nz/file/X35QmRbB#JFbClZhIZ5j8LmTUXdnvN97hbXCpCdrdXYkJdQyxNxQ",
        10: "https://mega.nz/file/qmQQlbgL#ZYG5jDqVD4bt1TXZOfYnsBmoEN37U72M4HUCqgDP8LI"
    }
}

REMOTE_GIT_DEPS = {
    4 : {
        0 : {
            1 : ('https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/100145',
                'https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/95067'),

            2 : ('https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/105727',
                'https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/105737'),

        },

        1 : {
          0 :  ('https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/121149',
                'https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/121152'),
          1 :  ('https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/128830',
              'https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/128833')
        },

        2 : {
            0 : ('https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/150344',
                'https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/150350'),
            1 : ('https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/161064',
                'https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/161079')
        },

        3 : {
            0 : ('https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/106744653',
                'https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/184190'),
            1: ('https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/106745181',
              'https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/196037')
        },

        4 : {
            0 : ('https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/106745722',
                'https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/210213'),
            1 : ('https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/106746517',
                'https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/220911'),
            2 : ('https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/106746981',
              'https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/233499'),
            3 : ('https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/106747303',
              'https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/239822')
        },

        5 : {
            0 : ('https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/106747982',
            'https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/266332'),
            1: ('https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/106748416',
              'https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/278528')
        }
    }
}