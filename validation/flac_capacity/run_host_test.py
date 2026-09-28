"""Compile and run the FLAC output-capacity host test.

Copies src/flac_decoder sources next to a host Audio.h shim, then builds them
with g++. The decoder source itself is not patched for the host.
"""

import hashlib
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src" / "flac_decoder"
SHIM = Path(__file__).resolve().parent / "shim" / "Audio.h"
TEST = Path(__file__).resolve().parent / "flac_capacity_test.cpp"
BUILD = Path(__file__).resolve().parent / "build"
COPY = BUILD / "src" / "flac_decoder"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def find_gxx() -> str:
    env = os.environ.get("CXX")
    if env:
        return env
    found = shutil.which("g++")
    if found:
        return found
    candidates = [
        Path(os.environ.get("TEMP", "")) / "w64devkit" / "bin" / "g++.exe",
        Path(os.environ.get("LOCALAPPDATA", "")) / "w64devkit" / "bin" / "g++.exe",
    ]
    for candidate in candidates:
        if candidate.is_file():
            return str(candidate)
    sys.exit("g++ not found. Set CXX or install a host g++.")


def main() -> int:
    if COPY.exists():
        shutil.rmtree(COPY)
    COPY.mkdir(parents=True)
    for name in ("flac_decoder.cpp", "flac_decoder.h"):
        shutil.copy2(SRC / name, COPY / name)
        if sha256(SRC / name) != sha256(COPY / name):
            sys.exit(f"copy mismatch: {name}")
    shim_dest = BUILD / "src" / "Audio.h"
    shutil.copy2(SHIM, shim_dest)

    exe = BUILD / "flac_capacity_test.exe"
    cmd = [
        find_gxx(),
        "-std=c++17",
        "-O2",
        "-Wall",
        "-Wextra",
        "-I",
        str(COPY),
        "-o",
        str(exe),
        str(TEST),
        str(COPY / "flac_decoder.cpp"),
    ]
    print(" ".join(cmd))
    compiled = subprocess.run(cmd)
    if compiled.returncode != 0:
        return compiled.returncode
    ran = subprocess.run([str(exe)])
    return ran.returncode


if __name__ == "__main__":
    raise SystemExit(main())
