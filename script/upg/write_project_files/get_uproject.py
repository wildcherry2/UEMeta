def get_uproject(major: int, minor: int, patch: int):
    out = {
        "FileVersion": 3,
        "EngineAssociation": f"{major}.{minor}.{patch}",
        "Category": "",
        "Description": "",
        "Modules": [
            {
                "Name": "MetadataHarness",
                "Type": "Runtime",
                "LoadingPhase": "Default"
            }
        ]
    }

    if (major == 4 and minor >= 0) or (major == 4 and minor <= 4):
        # noinspection bad-index
        out["Plugins"] = [
            {
                "Name": "OculusRift",
                "Enabled": False
            }
        ]

    return out