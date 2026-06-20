# Rather than build strictly in the parser's cmake or a python subcomponent, this file can act as a universal
# build script for any subcomponent that needs to build the proto files. This also allows all subprojects
# to share the same Buf binaries.
# FOR THIS TO WORK PROPERLY, Python 3.14 is required, and the script's cwd must be in the project root.

from pathlib import Path
from argparse import ArgumentParser
from subprocess import run
from urllib import request
import sys
import json

# module -> file map
MODULES = {
    "parser": ["proto/files/parser.proto"],
}

# fetch Buf binaries if needed
match sys.platform:
    case "win32":
        buf_url = r"https://github.com/bufbuild/buf/releases/download/v1.71.0/buf-Windows-x86_64.exe"
        buf_path = Path("proto/buf.exe")
    case "linux":
        buf_url = r"https://github.com/bufbuild/buf/releases/download/v1.71.0/buf-Linux-x86_64"
        buf_path = Path("proto/buf")
    case _:
        raise ValueError(f"Unsupported platform: {sys.platform}")

if not buf_path.exists():
    req = request.Request(buf_url, headers={
        "User-Agent": "curl",
        "Accept": "application/octet-stream",
    })

    with request.urlopen(req) as response:
        if response.status != 200:
            raise RuntimeError(f"Failed to fetch Buf binary: {response.status} {response.reason}")
        
        with open(buf_path, "wb") as f:
            f.write(response.read())
        
        # make the binary executable
        buf_path.chmod(0o755)

# parse arguments for language and output directory
parser = ArgumentParser()
parser.add_argument("--language", type=str, default="cpp",
                    help="The language to generate code for. E.g. 'cpp', 'python', 'java', etc.")
parser.add_argument("--output", type=Path, default="parser/intermediate/generated",
                    help="The directory to output the generated code to.")
parser.add_argument("--modules", type=str, nargs="+", default=["parser"], choices=["parser", "union"],
                    help="The protos you need to compile.")
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
            "remote": f"buf.build/protocolbuffers/{args.language}:v35.1",
            "out": str(args.output.resolve())
        }
    ]
})

# build argv
argv = [
    str(buf_path.resolve()),
    "generate",
    "proto",
    "--template",
    generate_template,
]

for module in args.modules:
    for proto in MODULES[module]:
        argv.append("--path")
        argv.append(str(proto))

# run the command
run(argv, text=True, check=True)