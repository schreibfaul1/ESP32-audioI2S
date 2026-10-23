#!/usr/bin/env python3
"""Test the actual MP3 32-bit leading-zero helper with full ASAN/UBSAN."""

import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def run(repo, build, compiler, source_ref):
    include = build / "include"
    header = include / "mp3_decoder/structs.h"
    header.parent.mkdir(parents=True, exist_ok=True)
    relative = "src/mp3_decoder/structs.h"
    data = (subprocess.check_output(["git", "show", f"{source_ref}:{relative}"], cwd=repo)
            if source_ref else (repo / relative).read_bytes())
    header.write_bytes(data)
    # This GCC-specific standard header is named differently on host Clang.
    (include / "stdint-gcc.h").write_text("#include <cstdint>\n")
    binary = build / "mp3-clz-test"
    subprocess.run([compiler, "-std=c++20", "-O1", "-g", "-fno-omit-frame-pointer",
                    "-fsanitize=address,undefined", "-I", str(include),
                    str(repo / "tests/mp3_clz_test.cpp"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True,
                   env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0:abort_on_error=1",
                        "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"})
    print("PASS: MP3 CLZ zero, all one-bit boundaries and 100000 bit patterns")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--source-ref", help="Existing Git revision for a pre-fix negative control")
    parser.add_argument("--cxx", default=os.environ.get("CXX", "clang++"))
    args = parser.parse_args()
    repo = Path(__file__).resolve().parent.parent
    if args.build_dir:
        build = args.build_dir.resolve()
        build.mkdir(parents=True, exist_ok=True)
        run(repo, build, args.cxx, args.source_ref)
    else:
        with tempfile.TemporaryDirectory(prefix="mp3-clz-test-") as directory:
            run(repo, Path(directory), args.cxx, args.source_ref)


if __name__ == "__main__":
    main()
