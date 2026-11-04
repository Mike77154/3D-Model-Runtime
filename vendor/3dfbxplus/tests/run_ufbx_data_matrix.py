#!/usr/bin/env python3
import os
import subprocess
import sys
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor

ROOT = Path(__file__).resolve().parents[1]
DATA = Path(os.environ.get("UFBX_DATA_DIR", str(ROOT.parent / "refs" / "ufbx-main" / "data")))
BUILD = Path(os.environ.get("BUILD_DIR", str(ROOT / "build")))
DFBX_PROBE = Path(os.environ.get("DFBX_PROBE", str(BUILD / "fbx_probe")))
UFBX_PROBE = os.environ.get("UFBX_PROBE")
MAX_FILES = int(os.environ.get("MAX_FILES", "0"))
INCLUDE_FUZZ = os.environ.get("INCLUDE_FUZZ", "0") not in ("0", "false", "False", "")
INCLUDE_EXPECTED_FAILS = os.environ.get("INCLUDE_EXPECTED_FAILS", "0") not in ("0", "false", "False", "")
OUT_DIR = Path(os.environ.get("OUT_DIR", str(ROOT / "tests" / "ufbx_matrix_out")))
PROBE_TIMEOUT = int(os.environ.get("PROBE_TIMEOUT", "20"))
WORKERS = int(os.environ.get("WORKERS", "1"))
COMPARE_KEYS = [
    "version", "is_binary", "meshes", "materials", "textures", "cameras", "lights",
    "lines", "curves", "surfaces", "embedded_textures", "embedded_bytes",
    "anim_clips", "anim_layers", "morph_targets",
]

def parse_kv(text):
    out = {}
    for line in text.splitlines():
        if "=" not in line:
            continue
        k, v = line.split("=", 1)
        out[k.strip()] = v.strip()
    return out

def run_probe(cmd, path):
    try:
        cp = subprocess.run([str(cmd), str(path)], text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=PROBE_TIMEOUT)
    except subprocess.TimeoutExpired:
        return 124, {"status": "timeout"}, "timeout"
    data = parse_kv(cp.stdout)
    if cp.returncode != 0 and "status" not in data:
        data["status"] = "exit_%d" % cp.returncode
    return cp.returncode, data, cp.stderr.strip()

def compare(ours, ref):
    mismatches = []
    if ref.get("status") == "ok" and ours.get("status") == "ok":
        for key in COMPARE_KEYS:
            if key in ours and key in ref and ours[key] != ref[key]:
                mismatches.append("%s:%s!=%s" % (key, ours[key], ref[key]))
        # Derived expectation: our curve_geometries includes line+nurbs curves.
        if "curve_geometries" in ours:
            try:
                ours_curves = int(ours["curve_geometries"])
                ref_curves = int(ref.get("lines", "0")) + int(ref.get("curves", "0"))
                if ours_curves < ref_curves:
                    mismatches.append("curve_geometries:%d<%d" % (ours_curves, ref_curves))
            except ValueError:
                pass
    return mismatches

def process_path(path):
    rc, ours, err = run_probe(DFBX_PROBE, path)
    ref = {}
    if UFBX_PROBE:
        rrc, ref, rerr = run_probe(UFBX_PROBE, path)
    mismatches = compare(ours, ref)
    return {
        "path": path,
        "ours": ours,
        "ref": ref,
        "mismatches": mismatches,
    }

def main():
    if not DATA.exists():
        print("missing data dir: %s" % DATA, file=sys.stderr)
        return 2
    if not DFBX_PROBE.exists():
        print("missing DFBX_PROBE: %s" % DFBX_PROBE, file=sys.stderr)
        return 2
    files = sorted(p for p in DATA.rglob("*.fbx")
                   if (INCLUDE_FUZZ or "fuzz" not in p.parts)
                   and (INCLUDE_EXPECTED_FAILS or "_fail_" not in p.name))
    if MAX_FILES > 0:
        files = files[:MAX_FILES]
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    manifest = OUT_DIR / "manifest.tsv"
    total = len(files)
    rows = []
    if WORKERS > 1:
        with ThreadPoolExecutor(max_workers=WORKERS) as ex:
            rows = list(ex.map(process_path, files))
    else:
        rows = [process_path(path) for path in files]
    dfbx_ok = 0
    ufbx_ok = 0
    hard_mismatch = 0
    with manifest.open("w", encoding="utf-8") as f:
        f.write("path\tdfbx_status\tufbx_status\tmismatches\n")
        for row in rows:
            ours = row["ours"]
            ref = row["ref"]
            mismatches = row["mismatches"]
            if ours.get("status") == "ok":
                dfbx_ok += 1
            if UFBX_PROBE and ref.get("status") == "ok":
                ufbx_ok += 1
            if mismatches:
                hard_mismatch += 1
            f.write("%s\t%s\t%s\t%s\n" % (
                row["path"].relative_to(DATA).as_posix(),
                ours.get("status", "unknown"),
                ref.get("status", "skip") if UFBX_PROBE else "skip",
                ";".join(mismatches)))
    print("ufbx matrix: total=%d dfbx_ok=%d" % (total, dfbx_ok))
    if UFBX_PROBE:
        print("ufbx matrix: ufbx_ok=%d mismatching_files=%d" % (ufbx_ok, hard_mismatch))
    print("manifest=%s" % manifest)
    return 0 if dfbx_ok == total else 1

if __name__ == "__main__":
    raise SystemExit(main())
