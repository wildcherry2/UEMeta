from pathlib import Path
from subprocess import run

def generate_proto():
    build = Path(__file__).resolve().parent.parent.parent / "proto" / "build.py"
    dest = Path(__file__).resolve().parent.parent / "proto"

    dest.mkdir(parents=True, exist_ok=True)
    run(["python", str(build), "--language", "python", "--output", str(dest)], check=True)

    with open(dest / "__init__.py", "w") as mod:
        mod.write(
            """
                from . import TopLevel_pb2
                from . import Enums_pb2
                from . import VersionedPrimitives_pb2
            """
        )