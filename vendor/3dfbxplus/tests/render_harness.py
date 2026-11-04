#!/usr/bin/env python3
"""Tiny dependency-free render harness for decode smoke testing.

It converts FBX to OBJ with the local tool, draws an orthographic wireframe PPM,
and writes SHA-256 hashes so CI can catch accidental geometry changes.
"""
from pathlib import Path
import hashlib
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
BUILD = Path(os.environ.get("BUILD_DIR", ROOT / "build"))
TO_OBJ = Path(os.environ.get("FBX_TO_OBJ", BUILD / "fbx_to_obj"))
OUT = ROOT / "tests" / "render_current"
W = 256
H = 256


def parse_obj(path):
    verts = []
    faces = []
    for line in path.read_text(errors="ignore").splitlines():
        if line.startswith("v "):
            _, x, y, z = line.split()[:4]
            verts.append((float(x), float(y), float(z)))
        elif line.startswith("f "):
            parts = line.split()[1:]
            idx = []
            for p in parts[:3]:
                idx.append(int(p.split("/")[0]) - 1)
            faces.append(tuple(idx))
    return verts, faces


def project(v, bounds):
    minx, maxx, miny, maxy = bounds
    x, y, _z = v
    sx = int((x - minx) / (maxx - minx or 1.0) * (W - 40) + 20)
    sy = int((y - miny) / (maxy - miny or 1.0) * (H - 40) + 20)
    return sx, H - 1 - sy


def line(img, a, b):
    x0, y0 = a
    x1, y1 = b
    dx = abs(x1 - x0)
    dy = -abs(y1 - y0)
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1
    err = dx + dy
    while True:
        if 0 <= x0 < W and 0 <= y0 < H:
            img[y0][x0] = 0
        if x0 == x1 and y0 == y1:
            break
        e2 = 2 * err
        if e2 >= dy:
            err += dy
            x0 += sx
        if e2 <= dx:
            err += dx
            y0 += sy


def render_obj(obj, ppm):
    verts, faces = parse_obj(obj)
    if not verts:
        raise RuntimeError(f"no vertices: {obj}")
    xs = [v[0] for v in verts]
    ys = [v[1] for v in verts]
    bounds = (min(xs), max(xs), min(ys), max(ys))
    img = [[255 for _ in range(W)] for _ in range(H)]
    pts = [project(v, bounds) for v in verts]
    for a, b, c in faces:
        line(img, pts[a], pts[b])
        line(img, pts[b], pts[c])
        line(img, pts[c], pts[a])
    data = bytearray(f"P5\n{W} {H}\n255\n", "ascii")
    for row in img:
        data.extend(row)
    ppm.write_bytes(data)
    return hashlib.sha256(data).hexdigest()


def main():
    if not TO_OBJ.exists():
        print(f"missing fbx_to_obj: {TO_OBJ}", file=sys.stderr)
        return 2
    OUT.mkdir(parents=True, exist_ok=True)
    subprocess.check_call([sys.executable, str(ROOT / "tests" / "gen_ascii_fbx_suite.py")])
    subprocess.check_call([sys.executable, str(ROOT / "tests" / "gen_version_matrix.py")])
    cases = sorted((ROOT / "tests" / "generated").glob("quad_7400_ascii.fbx"))
    for name in (
        "quad_fbx5_5000_binary_raw.fbx",
        "quad_2010_6100_ascii.fbx",
        "quad_2014_7400_binary_zlib.fbx",
        "quad_2016_7500_binary_zlib.fbx",
    ):
        path = ROOT / "tests" / "version_matrix" / name
        if path.exists():
            cases.append(path)
    hashes = []
    for fbx in cases:
        obj = OUT / (fbx.stem + ".obj")
        ppm = OUT / (fbx.stem + ".pgm")
        subprocess.check_call([str(TO_OBJ), str(fbx), str(obj)])
        digest = render_obj(obj, ppm)
        hashes.append(f"{fbx.name} {digest}\n")
    (OUT / "hashes.txt").write_text("".join(hashes))
    print(f"render harness OK: {len(hashes)} images")
    return 0


if __name__ == "__main__":
    sys.exit(main())
