
def get_metadata_analysis(major: int, minor: int, patch: int, include_list: list[str]):
    includes = "".join(f"#include \"{header.replace("\\", "/")}\"\n" for header in include_list)
    if (major == 4 and minor > 14) or major >= 5:
        return f"""
                    #include "MetadataHarness.h"
                    {includes}
                """

    if major == 4 and minor == 14:
        return f"""
                    #include "MetadataHarness.h"
                    {includes}
                """.replace("CoreMinimal.h", "Core.h")

    if major == 4 and minor < 14:
        return f"""
                    #include "MetadataHarness.h"
                    {includes}
                """.replace("CoreMinimal.h", "Core.h").replace("UObject/Object.h", "CoreUObject.h")

    raise Exception(f"Unknown version {major}.{minor}.{patch}")