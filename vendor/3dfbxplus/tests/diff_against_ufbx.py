#!/usr/bin/env python3
"""Optional differential smoke harness against a mature FBX parser.

Set UFBX_DUMP to an executable that prints stable scene counts for an FBX file.
The script compares 3dfbx's fbx_dump against that reference on every file in a
corpus directory. It is intentionally optional so the core C89 build remains
self-contained and dependency-free.
"""
from pathlib import Path
import os
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
BUILD = Path(os.environ.get("BUILD_DIR", ROOT / "build"))
OUR_DUMP = Path(os.environ.get("DUMP", BUILD / "fbx_dump"))
UFBX_DUMP = os.environ.get("UFBX_DUMP")
CORPUS = Path(os.environ.get("FBX_CORPUS", ROOT / "tests" / "generated"))


def scene_counts(text):
    out = {}
    for key in ("models", "meshes"):
        m = re.search(rf"{key}:\s+(\d+)", text)
        if m:
            out[key] = int(m.group(1))
    m = re.search(r"version:\s+(\d+)", text)
    if m:
        out["version"] = int(m.group(1))
    return out


def run(cmd, path):
    return subprocess.check_output([str(cmd), str(path)], stderr=subprocess.STDOUT, text=True, errors="replace")


def main():
    if not UFBX_DUMP:
        print("SKIP: set UFBX_DUMP=/path/to/reference_dump for differential testing")
        return 0
    if not OUR_DUMP.exists():
        print(f"missing fbx_dump: {OUR_DUMP}", file=sys.stderr)
        return 2
    files = sorted(CORPUS.rglob("*.fbx"))
    if not files:
        print(f"no FBX files under {CORPUS}", file=sys.stderr)
        return 2
    checked = 0
    for f in files:
        ours = scene_counts(run(OUR_DUMP, f))
        ref = scene_counts(run(UFBX_DUMP, f))
        for key in ("version", "models", "meshes"):
            if key in ours and key in ref and ours[key] != ref[key]:
                print(f"DIFF {f}: {key}: ours={ours[key]} ref={ref[key]}", file=sys.stderr)
                return 1
        checked += 1
    print(f"differential OK: {checked} files")
    return 0


if __name__ == "__main__":
    sys.exit(main())
