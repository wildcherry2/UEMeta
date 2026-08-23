# Rather than build strictly in the parser's cmake or a python subcomponent, this file can act as a universal
# build script for any subcomponent that needs to build the proto files. This also allows all subprojects
# to share the same Buf binaries.
# FOR THIS TO WORK PROPERLY, Python 3.14 is required, and the script's cwd must be in the project root.

from pathlib import Path
from argparse import ArgumentParser
from subprocess import run
from urllib import request
from io import BytesIO
from zipfile import ZipFile
import hashlib
import sys
import json

# Keep build tools beside this script so they can be shared by every consumer
# without requiring a system-wide installation.
tools_dir = Path(__file__).resolve().parent
protoc_version = "35.1"

match sys.platform:
    case "win32":
        buf_url = r"https://github.com/bufbuild/buf/releases/download/v1.71.0/buf-Windows-x86_64.exe"
        buf_path = tools_dir / "buf.exe"
        protoc_url = f"https://github.com/protocolbuffers/protobuf/releases/download/v{protoc_version}/protoc-{protoc_version}-win64.zip"
        protoc_sha256 = "5d3ff218d7d91eea95f7569bcb5a98f3030f8996d44151279d9772edcff76082"
        protoc_archive_path = "bin/protoc.exe"
        protoc_path = tools_dir / "protoc.exe"
    case "linux":
        buf_url = r"https://github.com/bufbuild/buf/releases/download/v1.71.0/buf-Linux-x86_64"
        buf_path = tools_dir / "buf"
        protoc_url = f"https://github.com/protocolbuffers/protobuf/releases/download/v{protoc_version}/protoc-{protoc_version}-linux-x86_64.zip"
        protoc_sha256 = "6930ebf62bd4ea607b98fff052596c6ee564b9835b4ce172c75a3f53ae9d91b7"
        protoc_archive_path = "bin/protoc"
        protoc_path = tools_dir / "protoc"
    case _:
        raise ValueError(f"Unsupported platform: {sys.platform}")

def download(url: str) -> bytes:
    req = request.Request(url, headers={
        "User-Agent": "curl",
        "Accept": "application/octet-stream",
    })

    with request.urlopen(req) as response:
        if response.status != 200:
            raise RuntimeError(f"Failed to fetch {url}: {response.status} {response.reason}")
        return response.read()

def write_executable(path: Path, contents: bytes) -> None:
    temporary_path = path.with_suffix(f"{path.suffix}.tmp")
    temporary_path.write_bytes(contents)
    temporary_path.chmod(0o755)
    temporary_path.replace(path)

if not buf_path.exists():
    write_executable(buf_path, download(buf_url))

if not protoc_path.exists():
    protoc_archive = download(protoc_url)
    actual_sha256 = hashlib.sha256(protoc_archive).hexdigest()
    if actual_sha256 != protoc_sha256:
        raise RuntimeError(
            f"protoc archive checksum mismatch: expected {protoc_sha256}, got {actual_sha256}"
        )

    with ZipFile(BytesIO(protoc_archive)) as archive:
        write_executable(protoc_path, archive.read(protoc_archive_path))

# parse arguments for language and output directory
parser = ArgumentParser()
parser.add_argument("--language", type=str, required=True,
                    help="The language to generate code for. E.g. 'cpp', 'python', 'java', etc.")
parser.add_argument("--output", type=Path, required=True,
                    help="The directory to output the generated code to.")
args = parser.parse_args()

# ensure output directory exists
args.output.mkdir(parents=True, exist_ok=True)

# run generate command for requested language and output directory
# first, make the template json for the Buf CLI
generate_template = json.dumps({
    "version": "v2",
    "clean": True,
    "managed": {
        "enabled": True
    },
    "plugins": [
        {
            "protoc_builtin": args.language,
            "protoc_path": str(protoc_path),
            "out": str(args.output.resolve())
        }
    ]
})

#todo this doesn't work
argv = [
    str(buf_path.resolve()),
    "dep",
    "update"
]

run(argv, text=True, check=True)

# build argv
argv = [
    str(buf_path.resolve()),
    "generate",
    "proto",
    "--template",
    generate_template,
    "--path",
    "proto/files/TopLevel.proto",
    "--path",
    "proto/files/Enums.proto",
    "--path",
    "proto/files/VersionedPrimitives.proto"
]

# run the command
run(argv, text=True, check=True)
