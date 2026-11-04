#!/usr/bin/env python3
"""Generate stable RE1 EMD animation frame strips.

Unlike the default diagnostic viewer, this script first asks the viewer for the
bounds of every requested frame, unions those bounds, then re-renders every frame
using one fixed bounding box. This keeps the camera/scale stable, so clip strips
show the real pose changes instead of per-frame auto-fit jitter.
"""
from __future__ import annotations

import argparse
import math
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Iterable, List, Tuple

from PIL import Image, ImageDraw

Bounds = Tuple[float, float, float, float, float, float]

BOUNDS_RE = re.compile(r"bounds:\s+([-+0-9.eE]+)\s+([-+0-9.eE]+)\s+([-+0-9.eE]+)\s+([-+0-9.eE]+)\s+([-+0-9.eE]+)\s+([-+0-9.eE]+)")
ANIM_RE = re.compile(r"anim applied: bank=([AB]) clip=(\d+) frame=(\d+)/(\d+) pose=(\d+) step=(\d+)")


def run_viewer(viewer: Path, emd: Path, out_bmp: Path, view: str, bank: str, clip: int, frame: int, extra: List[str]) -> subprocess.CompletedProcess[str]:
    cmd = [str(viewer), str(emd), str(out_bmp), view, "--anim", "--bank", bank, "--clip", str(clip), "--frame", str(frame)] + extra
    return subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)


def get_frame_count(viewer: Path, emd: Path, view: str, bank: str, clip: int) -> int:
    with tempfile.TemporaryDirectory() as td:
        out = Path(td) / "probe.bmp"
        cp = run_viewer(viewer, emd, out, view, bank, clip, 0, ["--dump-bounds"])
    m = ANIM_RE.search(cp.stderr)
    if not m:
        raise RuntimeError(f"Could not parse frame count for {bank}{clip}:\n{cp.stderr}")
    return int(m.group(4))


def get_bounds(viewer: Path, emd: Path, view: str, bank: str, clip: int, frame: int) -> Bounds:
    with tempfile.TemporaryDirectory() as td:
        out = Path(td) / "probe.bmp"
        cp = run_viewer(viewer, emd, out, view, bank, clip, frame, ["--dump-bounds"])
    m = BOUNDS_RE.search(cp.stderr)
    if not m:
        raise RuntimeError(f"Could not parse bounds for {bank}{clip} frame {frame}:\n{cp.stderr}")
    return tuple(float(x) for x in m.groups())  # type: ignore[return-value]


def union_bounds(all_bounds: Iterable[Bounds], margin: float = 0.06) -> Bounds:
    b = list(all_bounds)
    if not b:
        raise ValueError("no bounds")
    mnx = min(x[0] for x in b); mny = min(x[1] for x in b); mnz = min(x[2] for x in b)
    mxx = max(x[3] for x in b); mxy = max(x[4] for x in b); mxz = max(x[5] for x in b)
    sx = max(1.0, mxx - mnx); sy = max(1.0, mxy - mny); sz = max(1.0, mxz - mnz)
    mnx -= sx * margin; mxx += sx * margin
    mny -= sy * margin; mxy += sy * margin
    mnz -= sz * margin; mxz += sz * margin
    return (mnx, mny, mnz, mxx, mxy, mxz)


def render_frame(viewer: Path, emd: Path, work: Path, view: str, bank: str, clip: int, frame: int, bounds: Bounds) -> Image.Image:
    out = work / f"{bank}_c{clip:02d}_f{frame:03d}.bmp"
    extra = ["--bounds"] + [f"{v:.6f}" for v in bounds]
    run_viewer(viewer, emd, out, view, bank, clip, frame, extra)
    im = Image.open(out).convert("RGB")
    return im


def make_strip(viewer: Path, emd: Path, out_png: Path, view: str, bank: str, clip: int, thumb_w: int, thumb_h: int, cols: int) -> None:
    frame_count = get_frame_count(viewer, emd, view, bank, clip)
    bounds = union_bounds(get_bounds(viewer, emd, view, bank, clip, f) for f in range(frame_count))
    rows = int(math.ceil(frame_count / float(cols)))
    label_h = 18
    canvas = Image.new("RGB", (cols * thumb_w, rows * (thumb_h + label_h)), (180, 196, 194))
    draw = ImageDraw.Draw(canvas)
    with tempfile.TemporaryDirectory() as td:
        work = Path(td)
        for f in range(frame_count):
            im = render_frame(viewer, emd, work, view, bank, clip, f, bounds)
            im.thumbnail((thumb_w, thumb_h), Image.Resampling.LANCZOS)
            x = (f % cols) * thumb_w + (thumb_w - im.width) // 2
            y = (f // cols) * (thumb_h + label_h) + label_h + (thumb_h - im.height) // 2
            canvas.paste(im, (x, y))
            draw.text(((f % cols) * thumb_w + 4, (f // cols) * (thumb_h + label_h) + 2), f"{bank}{clip:02d} f{f:02d}", fill=(0, 0, 0))
    out_png.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(out_png)


def make_atlas(viewer: Path, emd: Path, out_png: Path, view: str, bank: str, clips: List[int], thumb_w: int, thumb_h: int, fast_bounds: bool = False) -> None:
    label_w = 118
    cols = 3
    label_h = 18
    row_h = thumb_h + label_h
    canvas = Image.new("RGB", (label_w + cols * thumb_w, len(clips) * row_h), (180, 196, 194))
    draw = ImageDraw.Draw(canvas)
    with tempfile.TemporaryDirectory() as td:
        work = Path(td)
        for row, clip in enumerate(clips):
            frame_count = get_frame_count(viewer, emd, view, bank, clip)
            picks = [0, frame_count // 2, max(0, frame_count - 1)]
            if fast_bounds:
                bounds = union_bounds(get_bounds(viewer, emd, view, bank, clip, f) for f in picks)
            else:
                bounds = union_bounds(get_bounds(viewer, emd, view, bank, clip, f) for f in range(frame_count))
            draw.text((4, row * row_h + 4), f"Bank {bank} Clip {clip:02d}\n{frame_count} frames", fill=(0, 0, 0))
            for col, frame in enumerate(picks):
                im = render_frame(viewer, emd, work, view, bank, clip, frame, bounds)
                im.thumbnail((thumb_w, thumb_h), Image.Resampling.LANCZOS)
                x0 = label_w + col * thumb_w
                y0 = row * row_h + label_h
                canvas.paste(im, (x0 + (thumb_w - im.width) // 2, y0 + (thumb_h - im.height) // 2))
                draw.text((x0 + 4, row * row_h + 2), f"f{frame:02d}", fill=(0, 0, 0))
    out_png.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(out_png)


def parse_clip_list(s: str) -> List[int]:
    out: List[int] = []
    for part in s.split(','):
        part = part.strip()
        if not part:
            continue
        if '-' in part:
            a, b = [int(x) for x in part.split('-', 1)]
            out.extend(range(a, b + 1))
        else:
            out.append(int(part))
    return out


def main(argv: List[str]) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("emd", type=Path)
    ap.add_argument("outdir", type=Path)
    ap.add_argument("--viewer", type=Path, default=Path("build/emd_textured_viewer"))
    ap.add_argument("--bank", default="B", choices=["A", "B"])
    ap.add_argument("--view", default="side", choices=["front", "side", "iso"])
    ap.add_argument("--clips", default="0-30")
    ap.add_argument("--strip-clips", default="0,1,8,10,11,13")
    ap.add_argument("--fast-atlas", action="store_true", help="use only start/mid/end for atlas bounds; strips still use all frames")
    args = ap.parse_args(argv)
    clips = parse_clip_list(args.clips)
    strip_clips = parse_clip_list(args.strip_clips)
    args.outdir.mkdir(parents=True, exist_ok=True)
    make_atlas(args.viewer, args.emd, args.outdir / f"{args.emd.stem}_BANK{args.bank}_{args.view}_stable_atlas.png", args.view, args.bank, clips, 220, 160, args.fast_atlas)
    for c in strip_clips:
        make_strip(args.viewer, args.emd, args.outdir / f"{args.emd.stem}_BANK{args.bank}_CLIP{c:02d}_{args.view}_stable_strip.png", args.view, args.bank, c, 160, 120, 8)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
