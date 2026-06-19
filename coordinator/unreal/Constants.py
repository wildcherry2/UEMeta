from re import RegexFlag
import re

CANONICAL_BRANCH_RE = re.compile(r"^(?P<version>(?P<major>\d+\.\d+)(\.(?P<patch>\d+))?)(-release)?$",
                                 RegexFlag.M | RegexFlag.U)

DEP_MAP = {
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
