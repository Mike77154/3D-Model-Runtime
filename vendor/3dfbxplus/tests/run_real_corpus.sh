#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR=${BUILD_DIR:-"$ROOT_DIR/build"}
DUMP=${DUMP:-"$BUILD_DIR/fbx_dump"}
STREAM=${STREAM:-"$BUILD_DIR/stream_smoke"}
STREAM_SCAN=${STREAM_SCAN:-"$BUILD_DIR/stream_scan_smoke"}
TO_ASCII=${TO_ASCII:-"$BUILD_DIR/fbx_to_fbxascii"}
TO_BIN=${TO_BIN:-"$BUILD_DIR/fbx_to_fbxbin"}
PREVIEW=${PREVIEW:-"$BUILD_DIR/fbx_preview_ppm"}
OUT="$ROOT_DIR/tests/real_corpus_out"
DIRS=${FBX_CORPUS_DIRS:-"$ROOT_DIR/tests/corpus/blender:$ROOT_DIR/tests/corpus/maya:$ROOT_DIR/tests/corpus/3dsmax:$ROOT_DIR/tests/corpus/mixamo"}

for tool in "$DUMP" "$STREAM" "$TO_ASCII" "$TO_BIN"; do
    if [ ! -x "$tool" ]; then
        echo "missing tool: $tool" >&2
        exit 2
    fi
done

rm -rf "$OUT"
mkdir -p "$OUT/dump" "$OUT/roundtrip" "$OUT/preview"
manifest="$OUT/manifest.tsv"
printf 'status\tpath\tdetail\n' > "$manifest"
count=0
fail=0

old_ifs=$IFS
IFS=:
for dir in $DIRS; do
    IFS=$old_ifs
    if [ ! -d "$dir" ]; then
        IFS=:
        continue
    fi
    find "$dir" -type f \( -name '*.fbx' -o -name '*.FBX' \) | while IFS= read -r f; do
        safe=$(printf '%s' "$f" | sed 's#[/: ]#_#g')
        if "$DUMP" "$f" > "$OUT/dump/$safe.txt" 2> "$OUT/dump/$safe.err"; then
            "$STREAM" "$f" 11 > "$OUT/dump/$safe.stream.txt" 2>> "$OUT/dump/$safe.err" || true
            if [ -x "$STREAM_SCAN" ]; then
                "$STREAM_SCAN" "$f" 11 > "$OUT/dump/$safe.stream_scan.txt" 2>> "$OUT/dump/$safe.err" || true
            fi
            "$TO_ASCII" "$f" "$OUT/roundtrip/$safe.ascii.fbx" 7400 > "$OUT/dump/$safe.to_ascii.log" 2>> "$OUT/dump/$safe.err" || true
            "$TO_BIN" "$f" "$OUT/roundtrip/$safe.bin.fbx" 7400 1 > "$OUT/dump/$safe.to_bin.log" 2>> "$OUT/dump/$safe.err" || true
            if [ -x "$PREVIEW" ] && [ ! -f "$OUT/preview/first.ppm" ]; then
                "$PREVIEW" "$f" "$OUT/preview/first.ppm" > "$OUT/preview/first.log" 2>> "$OUT/dump/$safe.err" || true
            fi
            printf 'OK\t%s\tdecoded\n' "$f" >> "$manifest"
        else
            printf 'FAIL\t%s\tdump failed\n' "$f" >> "$manifest"
        fi
    done
    IFS=:
done
IFS=$old_ifs

count=$(grep -c '^OK\|^FAIL' "$manifest" || true)
fail=$(grep -c '^FAIL' "$manifest" || true)
if [ "$count" -eq 0 ]; then
    echo "real corpus skipped: no FBX files found; see tests/corpus/README.md"
    exit 0
fi
if [ "$fail" -ne 0 ]; then
    echo "real corpus completed with failures: $fail / $count"
    exit 1
fi

echo "real corpus OK: $count files"
