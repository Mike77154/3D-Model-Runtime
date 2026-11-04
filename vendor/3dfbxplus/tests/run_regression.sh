#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR=${BUILD_DIR:-"$ROOT_DIR/build"}
DUMP=${DUMP:-"$BUILD_DIR/fbx_dump"}
BASELINE="$ROOT_DIR/tests/baseline"
OUT="$ROOT_DIR/tests/current"

if [ ! -x "$DUMP" ]; then
    echo "fbx_dump not found: $DUMP" >&2
    echo "hint: build first, or pass BUILD_DIR=/path/to/build" >&2
    exit 2
fi

rm -rf "$OUT"
mkdir -p "$OUT"
cd "$ROOT_DIR"

./tests/gen_ascii_fbx_suite.py >/dev/null
./tests/gen_binary_compressed_fbx.py >/dev/null

for f in tests/generated/*.fbx; do
    name=$(basename "$f")
    "$DUMP" "$f" > "$OUT/$name.txt"
done

for b in "$BASELINE"/*.txt; do
    name=$(basename "$b")
    if ! diff -u "$b" "$OUT/$name" > "$OUT/diff_$name"; then
        echo "regression diff: $name" >&2
        exit 1
    fi
    rm -f "$OUT/diff_$name"
done

echo "regression OK"
