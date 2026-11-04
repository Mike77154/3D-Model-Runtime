#!/usr/bin/env python3
"""Tiny crash-smoke fuzzer for the FBX importer.

It mutates the generated ASCII corpus and only checks the key safety property:
fbx_dump must exit normally or fail cleanly, never crash via signal.
"""
import os
import random
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DUMP = Path(os.environ.get("DUMP", ROOT / "build" / "fbx_dump"))
CORPUS = ROOT / "tests" / "generated"


def mutate(data: bytearray, rng: random.Random) -> bytearray:
    out = bytearray(data)
    if not out:
        return out
    ops = rng.randint(1, 32)
    for _ in range(ops):
        op = rng.randrange(4)
        if op == 0 and out:
            out[rng.randrange(len(out))] = rng.randrange(256)
        elif op == 1 and len(out) > 1:
            start = rng.randrange(len(out))
            end = min(len(out), start + rng.randrange(1, 64))
            del out[start:end]
        elif op == 2:
            pos = rng.randrange(len(out) + 1)
            n = rng.randrange(1, 64)
            out[pos:pos] = bytes(rng.randrange(256) for _ in range(n))
        elif op == 3 and out:
            pos = rng.randrange(len(out))
            out[pos:pos] = out[max(0, pos - 16):pos]
    return out


def main() -> int:
    if not DUMP.exists():
        print(f"fbx_dump not found: {DUMP}", file=sys.stderr)
        return 2
    files = sorted(CORPUS.glob("*.fbx"))
    if not files:
        print(f"no corpus files in {CORPUS}", file=sys.stderr)
        return 2
    rng = random.Random(0x3DFB)
    cases = int(os.environ.get("CASES", "96"))
    with tempfile.TemporaryDirectory(prefix="3dfbx_fuzz_") as tmp:
        tmp_path = Path(tmp)
        for i in range(cases):
            src = files[i % len(files)]
            data = mutate(bytearray(src.read_bytes()), rng)
            p = tmp_path / f"case_{i:04d}.fbx"
            p.write_bytes(data)
            try:
                r = subprocess.run([str(DUMP), str(p)], stdout=subprocess.DEVNULL,
                                   stderr=subprocess.DEVNULL, timeout=2.0)
            except subprocess.TimeoutExpired:
                print(f"timeout: {p}", file=sys.stderr)
                return 1
            if r.returncode < 0:
                print(f"crash: {p} signal {-r.returncode}", file=sys.stderr)
                return 1
    print(f"fuzz smoke OK ({cases} cases)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
