from pathlib import Path

def __merge_proc(out_path: str, proto_paths: list[str]) -> bool:
    from MergeImpl import Merger # we import here because this is called from worker processes; we don't need to import in the main process
    return Merger.merge(Path(out_path), proto_paths)

def merge(input_dir: Path, output_dir: Path):
    import os
    from collections import defaultdict
    from functools import partial
    from multiprocessing.pool import Pool

    if not input_dir.is_dir():
        raise Exception(f"(merge) Failed to merge: {input_dir} is not a directory!")

    output_dir.mkdir(parents=True, exist_ok=True)

    version_map: dict[str, list[str]] = defaultdict(list)

    extensions = (".functionbin", ".recordbin", ".enumbin", ".varbin", ".declbin")
    for root, dir, files in os.walk(input_dir):
        for file in files:
            if file.endswith(extensions):
                path = os.path.join(root, file)
                version_map[file].append(path)

    bound_merge = partial(__merge_proc, output_dir)
    with Pool() as pool:
        cpu_count = os.process_cpu_count()
        if cpu_count is None: cpu_count = 1
        results = pool.imap_unordered(bound_merge, version_map.values(), max(1, int(len(version_map) / cpu_count)))
        from collections import deque
        deque(results, maxlen=0)