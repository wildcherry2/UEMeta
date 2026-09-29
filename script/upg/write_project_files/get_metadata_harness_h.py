
def get_metadata_harness_h(major: int, minor: int):
    if major == 4 and minor == 14:
        return """
                    #pragma once
                    #include "Core.h"
                    #include "UObject/ObjectMacros.h"
                """
    else:
        return """
                    #pragma once
                    #include "CoreMinimal.h"
                """