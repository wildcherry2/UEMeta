from re import RegexFlag
import re

CANONICAL_BRANCH_RE = re.compile(r"^(?P<version>(?P<major>\d+)\.(?P<minor>\d+)(\.(?P<patch>\d+))?)(?P<label>-release)?$",
                                 RegexFlag.M | RegexFlag.U)

GIT_DEP_MAP = {
    '4.0.1': ('https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/100145',
              'https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/95067'),
    '4.0.2': ('https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/105727',
              'https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/105737'),
    '4.1': ('https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/121149',
            'https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/121152'),
    '4.1.1': ('https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/128830',
              'https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/128833'),
    '4.2': ('https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/150344',
            'https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/150350'),
    '4.2.1': ('https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/161064',
              'https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/161079'),
    '4.3': ('https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/106744653',
            'https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/184190'),
    '4.3.1': ('https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/106745181',
              'https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/196037'),
    '4.4': ('https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/106745722',
            'https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/210213'),
    '4.4.1': ('https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/106746517',
              'https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/220911'),
    '4.4.2': ('https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/106746981',
              'https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/233499'),
    '4.4.3': ('https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/106747303',
              'https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/239822'),
    '4.5': ('https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/106747982',
            'https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/266332'),
    '4.5.1': ('https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/106748416',
              'https://api.github.com/repos/EpicGames/UnrealEngine/releases/assets/278528')
}

UE_CDN_MAP = {
    "4.6": "https://mega.nz/file/3nphwapA#wQhpOz03GNXc0zZCIge673O8sNvOb-LDK_W1NnvrUe0",
    "4.7": "https://mega.nz/file/Kq5kjZxR#ojrVU8sRXrV3KF_pNPLcFXIihg-qm5CMRJOCBTKrd7U",
    "4.8": "https://mega.nz/file/Xj5mXCYK#UT1QIteq7HgxAHBnOJVLlmVT_kf4xHjEIt5F9plmAVE",
    "4.9": "https://mega.nz/file/X35QmRbB#JFbClZhIZ5j8LmTUXdnvN97hbXCpCdrdXYkJdQyxNxQ",
    "4.10": "https://mega.nz/file/qmQQlbgL#ZYG5jDqVD4bt1TXZOfYnsBmoEN37U72M4HUCqgDP8LI"
}
