#!/usr/bin/env python3
"""Build the real decoder sources with host-only platform adapters."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def source_bytes(repo, relative, ref):
    if ref:
        return subprocess.check_output(["git", "show", f"{ref}:{relative}"], cwd=repo)
    return (repo / relative).read_bytes()


def run_tests(repo, build, ref, expect_leak, compiler):
    host = repo / "tests/host"
    overlay = build / "src"
    faad = overlay / "aac_decoder/libfaad"
    faad.mkdir(parents=True, exist_ok=True)
    source = repo / "src/aac_decoder/libfaad"
    for path in sorted(source.iterdir()):
        if path.suffix not in (".cpp", ".h"):
            continue
        data = source_bytes(repo, path.relative_to(repo).as_posix(), ref)
        if path.name == "neaacdec.h":
            # ESP32 long is 32 bits. The implementation already returns int32_t;
            # LP64 hosts need the matching declaration in this isolated copy.
            old = b"long                     NeAACDecInit("
            if data.count(old) != 1:
                raise RuntimeError("Unexpected NeAACDecInit declaration")
            data = data.replace(old, b"int32_t                  NeAACDecInit(")
        (faad / path.name).write_bytes(data)
    (overlay / "psram_unique_ptr.hpp").write_bytes(source_bytes(repo, "src/psram_unique_ptr.hpp", ref))
    shutil.copyfile(host / "Audio.h", overlay / "Audio.h")

    common = [compiler, "-std=c++20", "-O1", "-g", "-fno-omit-frame-pointer",
              "-fsanitize=address,undefined", "-fdelayed-template-parsing",
              "-DCONFIG_IDF_TARGET_ESP32S3", "-Wno-unknown-pragmas", "-Wno-format",
              "-Wno-unused-value", "-I", str(overlay), "-I", str(host), "-I", str(faad)]
    environment = {**os.environ, "ASAN_OPTIONS": "detect_leaks=0:abort_on_error=1",
                   "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"}
    for profile in ("float", "fixed"):
        executable = build / f"aac-lifecycle-{profile}"
        command = common + (["-DFIXED_POINT"] if profile == "fixed" else [])
        command += [str(faad / "neaacdec.cpp"), str(host / "Allocations.cpp"),
                    str(repo / "tests/aac_decoder_lifecycle_test.cpp"), "-o", str(executable)]
        subprocess.run(command, check=True)
        subprocess.run([str(executable)] + (["--expect-leak"] if expect_leak else []),
                       check=True, env=environment)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, help="Keep build outputs in this directory")
    parser.add_argument("--source-ref", help="Read decoder sources from an existing Git ref")
    parser.add_argument("--expect-leak", action="store_true", help="Reproduce the pre-fix FAAD leak")
    parser.add_argument("--cxx", default=os.environ.get("CXX", "clang++"), help="Clang C++20 compiler")
    args = parser.parse_args()
    repo = Path(__file__).resolve().parent.parent
    if args.expect_leak and not args.source_ref:
        parser.error("--expect-leak requires an explicit --source-ref")
    if args.build_dir:
        build = args.build_dir.resolve()
        build.mkdir(parents=True, exist_ok=True)
        run_tests(repo, build, args.source_ref, args.expect_leak, args.cxx)
    else:
        with tempfile.TemporaryDirectory(prefix="audioi2s-host-") as directory:
            run_tests(repo, Path(directory), args.source_ref, args.expect_leak, args.cxx)


if __name__ == "__main__":
    main()
