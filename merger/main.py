from pathlib import Path

def merge(out_path: str, proto_paths: list[str]) -> bool:
    from Merger import Merger # we import here because this is called from worker processes; we don't need to import in the main process
    return Merger(Path(out_path), proto_paths).merge()

if __name__== "__main__":
    import os
    import sys
    from argparse import ArgumentParser
    from collections import defaultdict
    from functools import partial
    from multiprocessing.pool import Pool
    from subprocess import run

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
                versions.append(entry.name) # todo combine with below for loop

    version_map: dict[str, list[str]] = defaultdict(list)

    for version in versions:
        with os.scandir(str(args.input / version)) as entries:
            for entry in entries:
                if entry.is_file():
                    version_map[entry.name].append(entry.path)

    bound_merge = partial(merge, args.output)
    with Pool() as pool:
        results = pool.imap_unordered(bound_merge, version_map.values(), max(1, int(len(version_map) / os.process_cpu_count())))
        from collections import deque
        deque(results, maxlen=0)