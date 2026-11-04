#!/usr/bin/env python3
"""Compare a RE1 EMD EDD/EMR bank against a RE1MV-exported .ANI text file.

This is a validation helper, not runtime code.  It mirrors RE1MV EMR2ANI:
  - hierarchy comes from EMR PartListOffset entries
  - pivots are exported as Obj_Pos.X*2, -Obj_Pos.Z*2, Obj_Pos.Y*2
  - frame root is Pos.Z*2, -Pos.X*2, Pos.Y*2
  - angles are exported for objects PC-1..0 as CF(360-Z), CF(X), CF(360-Y)
"""
from __future__ import annotations

import argparse
import struct
from pathlib import Path
from typing import List, Tuple

Triple = Tuple[float, float, float]


def rd16(b: bytes, off: int) -> int:
    return struct.unpack_from('<H', b, off)[0]


def rd32(b: bytes, off: int) -> int:
    return struct.unpack_from('<I', b, off)[0]


def calc_pos(w: int) -> float:
    w &= 0xFFFF
    if w > 32767:
        return (65535 - w) / 100.0
    return -w / 100.0


def angle_deg(w: int) -> float:
    return ((w & 0x0FFF) / 0x0FFF) * 360.0


def cf(v: float) -> float:
    return v if v < 180.0 else v - 360.0


def sections_re1_emd(data: bytes) -> List[Tuple[int, int]]:
    if len(data) < 20:
        raise ValueError('file too small')
    dir_start = len(data) - 20
    offs = [rd32(data, dir_start + i * 4) for i in range(5)]
    return [
        (0, offs[0]),
        (offs[0], offs[1] - offs[0]),
        (offs[1], offs[2] - offs[1]),
        (offs[2], offs[3] - offs[2]),
        (offs[3], offs[4] - offs[3]),
        (offs[4], dir_start - offs[4]),
    ]


def parse_edd(edd: bytes):
    first_stream = rd16(edd, 2)
    clip_count = first_stream // 4
    clips = []
    for c in range(clip_count):
        frames = rd16(edd, c * 4)
        stream = rd16(edd, c * 4 + 2)
        poses = []
        for f in range(frames):
            rec = stream + f * 4
            poses.append((rd16(edd, rec), rd16(edd, rec + 2)))
        clips.append((frames, stream, poses))
    return clips


def parse_emr(emr: bytes):
    partlist = rd16(emr, 0)
    animoff = rd16(emr, 2)
    objcount = rd16(emr, 4)
    framesize = rd16(emr, 6)
    tree = []
    for i in range(objcount):
        count = rd16(emr, partlist + i * 4)
        off = rd16(emr, partlist + i * 4 + 2)
        tree.append([emr[partlist + off + j] for j in range(count)])
    pivots = []
    for i in range(objcount):
        s = 8 + i * 6
        obj_z = calc_pos(rd16(emr, s + 0))
        obj_y = calc_pos(rd16(emr, s + 2))
        obj_x = calc_pos(rd16(emr, s + 4))
        pivots.append((obj_x * 2.0, -obj_z * 2.0, obj_y * 2.0))
    frame_count = ((len(emr) - 4) - animoff) // framesize
    frames = []
    for f in range(frame_count):
        off = animoff + f * framesize
        px = calc_pos(rd16(emr, off + 0))
        py = calc_pos(rd16(emr, off + 2))
        pz = calc_pos(rd16(emr, off + 4))
        root = (pz * 2.0, -px * 2.0, py * 2.0)
        angles_raw = []
        for i in range(objcount):
            aoff = off + 12 + i * 6
            ax = angle_deg(rd16(emr, aoff + 0))
            ay = angle_deg(rd16(emr, aoff + 2))
            az = angle_deg(rd16(emr, aoff + 4))
            angles_raw.append((ax, ay, az))
        angles = []
        for i in range(objcount - 1, -1, -1):
            ax, ay, az = angles_raw[i]
            angles.append((cf(360.0 - az), cf(ax), cf(360.0 - ay)))
        frames.append((root, angles))
    return {'tree': tree, 'pivots': pivots, 'frames': frames, 'objcount': objcount}


def parse_ani(path: Path):
    lines = [ln.strip() for ln in path.read_text().replace('\r', '').split('\n') if ln.strip()]
    i = 0
    pc = int(lines[i]); i += 1
    tree = []
    for _ in range(pc):
        vals = [int(x) for x in lines[i].split()]
        i += 1
        tree.append(vals[1:])
    pivots = []
    for _ in range(pc):
        pivots.append(tuple(float(x) for x in lines[i].split()))
        i += 1
    frame_count = int(lines[i]); i += 1
    frames = []
    for _ in range(frame_count):
        root = tuple(float(x) for x in lines[i].split()); i += 1
        angles = []
        for _ in range(pc):
            angles.append(tuple(float(x) for x in lines[i].split()))
            i += 1
        frames.append((root, angles))
    return {'tree': tree, 'pivots': pivots, 'frames': frames, 'objcount': pc}


def max_triple_delta(a: Triple, b: Triple) -> float:
    return max(abs(a[j] - b[j]) for j in range(3))


def compare(emd: Path, ani: Path, bank: str, clip: int | None) -> None:
    data = emd.read_bytes()
    secs = sections_re1_emd(data)
    if bank.upper() == 'A':
        emr = data[secs[0][0]:secs[0][0] + secs[0][1]]
        edd = data[secs[1][0]:secs[1][0] + secs[1][1]]
    else:
        emr = data[secs[2][0]:secs[2][0] + secs[2][1]]
        edd = data[secs[3][0]:secs[3][0] + secs[3][1]]
    emr_s = parse_emr(emr)
    ani_s = parse_ani(ani)
    if clip is None:
        frame_ids = list(range(len(emr_s['frames'])))
    else:
        clips = parse_edd(edd)
        frame_ids = [pose for pose, _step in clips[clip][2]]
    print(f'emd={emd}')
    print(f'ani={ani}')
    print(f'bank={bank.upper()} clip={"ALL_EMR_FRAMES" if clip is None else clip}')
    print(f'objects: emd={emr_s["objcount"]} ani={ani_s["objcount"]}')
    print(f'tree_match={emr_s["tree"] == ani_s["tree"]}')
    print(f'frame_count: emd_selection={len(frame_ids)} ani={len(ani_s["frames"])}')
    max_delta = 0.0
    where = 'none'
    for i, (a, b) in enumerate(zip(emr_s['pivots'], ani_s['pivots'])):
        d = max_triple_delta(a, b)
        if d > max_delta:
            max_delta = d; where = f'pivot[{i}]'
    for out_i, pose_i in enumerate(frame_ids[:len(ani_s['frames'])]):
        root, angles = emr_s['frames'][pose_i]
        ani_root, ani_angles = ani_s['frames'][out_i]
        d = max_triple_delta(root, ani_root)
        if d > max_delta:
            max_delta = d; where = f'frame[{out_i}] root pose={pose_i}'
        for j, (ea, aa) in enumerate(zip(angles, ani_angles)):
            d = max_triple_delta(ea, aa)
            if d > max_delta:
                max_delta = d; where = f'frame[{out_i}] angle_line[{j}] pose={pose_i}'
    print(f'max_abs_delta={max_delta:.6f} at {where}')
    print('status=' + ('OK' if len(frame_ids) == len(ani_s['frames']) and max_delta <= 0.001 else 'CHECK'))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('emd', type=Path)
    ap.add_argument('ani', type=Path)
    ap.add_argument('--bank', choices=['A', 'B'], default='B')
    ap.add_argument('--clip', type=int, default=None, help='compare ANI against one EDD clip; omit for all EMR frames')
    args = ap.parse_args()
    compare(args.emd, args.ani, args.bank, args.clip)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
