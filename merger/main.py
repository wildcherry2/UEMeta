import os
import re
import sys
from argparse import ArgumentParser
from collections import defaultdict
from dataclasses import dataclass
from multiprocessing.pool import Pool
from pathlib import Path
from subprocess import run

@dataclass
class MergeArgs:
    version_file_list: list[str]
    input_dir: Path
    output_dir: Path

def merge(merge_args: MergeArgs) -> bool:
    from Merger import Merger # we import here because this is called from worker processes; we don't need to import in the main process
    return Merger(merge_args.version_file_list, merge_args.input_dir, merge_args.output_dir).merge()

if __name__== "__main__":
    parser = ArgumentParser()
    parser.add_argument("--output", type=Path, required=True,
                        help="The directory to output the generated code to.")
    parser.add_argument("--input", type=Path, required=True,
                        help="The directory that contains versioned files from the parser.")
    args = parser.parse_args()

    if not args.input.exists():
        raise FileNotFoundError(f"The input directory {args.input} does not exist.")

    if not args.input.is_dir():
        raise NotADirectoryError(f"The input directory {args.input} is not a directory.")

    project_dir = Path(__file__).resolve().parent
    proto_build = project_dir.parent / "proto" / "build.py"
    proto_dir = project_dir / "proto"
    proto_dir.mkdir(parents=True, exist_ok=True)

    # generate protos
    run(["python", str(proto_build), "--language", "python", "--output", str(proto_dir)])
    sys.path.append(str(proto_dir))

    versions: list[str] = []

    with os.scandir(str(args.input)) as entries:
        for entry in entries:
            if entry.is_dir():
                versions.append(entry.name)

    version_map: dict[str, list[str]] = defaultdict(list)
    identity_regex = re.compile(r"^(?P<hash>\d+)-.+")

    for version in versions:
        with os.scandir(str(args.input / version)) as entries:
            for entry in entries:
                if entry.is_file():
                    hash_match = identity_regex.match(entry.name)
                    # todo proper logging on error
                    if hash_match is not None:
                        version_map[hash_match.group("hash")].append(entry.name)

    with Pool() as pool:
        results = pool.imap_unordered(merge, version_map.values(), int(len(version_map) / os.process_cpu_count()))