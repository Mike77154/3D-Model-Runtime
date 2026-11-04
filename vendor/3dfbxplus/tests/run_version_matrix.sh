#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR=${BUILD_DIR:-"$ROOT_DIR/build"}
DUMP=${DUMP:-"$BUILD_DIR/fbx_dump"}
TO_ASCII=${TO_ASCII:-"$BUILD_DIR/fbx_to_fbxascii"}
TO_BIN=${TO_BIN:-"$BUILD_DIR/fbx_to_fbxbin"}
STREAM=${STREAM:-"$BUILD_DIR/stream_smoke"}
STREAM_SCAN=${STREAM_SCAN:-"$BUILD_DIR/stream_scan_smoke"}
PREVIEW=${PREVIEW:-"$BUILD_DIR/fbx_preview_ppm"}
OUT="$ROOT_DIR/tests/version_matrix_out"

for tool in "$DUMP" "$TO_ASCII" "$TO_BIN" "$STREAM" "$STREAM_SCAN"; do
    if [ ! -x "$tool" ]; then
        echo "missing tool: $tool" >&2
        exit 2
    fi
done

cd "$ROOT_DIR"
./tests/gen_version_matrix.py >/dev/null
rm -rf "$OUT"
mkdir -p "$OUT/dump" "$OUT/ascii" "$OUT/bin" "$OUT/preview"

count=0
for f in tests/version_matrix/*.fbx; do
    name=$(basename "$f")
    ver=$(printf '%s\n' "$name" | sed -n 's/.*_\([0-9][0-9][0-9][0-9]\)_.*/\1/p')
    if [ -z "$ver" ]; then ver=7400; fi
    "$DUMP" "$f" > "$OUT/dump/$name.txt"
    case "$name" in
        *_binary_*)
            "$STREAM" "$f" 5 > "$OUT/dump/$name.stream.txt"
            "$STREAM_SCAN" "$f" 5 > "$OUT/dump/$name.stream_scan.txt"
            ;;
        *)
            "$STREAM" "$f" 5 > "$OUT/dump/$name.stream.txt"
            printf "scan skipped: ascii input\n" > "$OUT/dump/$name.stream_scan.txt"
            ;;
    esac
    "$TO_ASCII" "$f" "$OUT/ascii/$name.round_ascii.fbx" "$ver" > "$OUT/dump/$name.to_ascii.log"
    "$DUMP" "$OUT/ascii/$name.round_ascii.fbx" > "$OUT/dump/$name.round_ascii.txt"
    "$TO_BIN" "$f" "$OUT/bin/$name.round_bin.fbx" "$ver" 1 > "$OUT/dump/$name.to_bin.log"
    "$DUMP" "$OUT/bin/$name.round_bin.fbx" > "$OUT/dump/$name.round_bin.txt"
    count=$((count + 1))
done

if [ -x "$PREVIEW" ]; then
    "$PREVIEW" tests/version_matrix/quad_2014_7400_ascii.fbx "$OUT/preview/quad_2014_7400.ppm" > "$OUT/preview/preview.log"
fi

echo "version matrix OK: $count source fixtures, decode-stream + scan-stream + ascii roundtrip + binary roundtrip"
