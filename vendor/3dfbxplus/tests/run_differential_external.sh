#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR=${BUILD_DIR:-"$ROOT_DIR/build"}
OUR_DUMP=${OUR_DUMP:-"$BUILD_DIR/fbx_dump"}
REF_DUMP=${REF_DUMP:-""}
CORPUS=${CORPUS:-"$ROOT_DIR/tests/version_matrix"}
OUT="$ROOT_DIR/tests/differential_out"

if [ -z "$REF_DUMP" ]; then
    echo "differential skipped: set REF_DUMP to a mature parser dump tool, e.g. a local ufbx-based dumper"
    exit 0
fi
if [ ! -x "$OUR_DUMP" ]; then
    echo "missing OUR_DUMP: $OUR_DUMP" >&2
    exit 2
fi
if [ ! -x "$REF_DUMP" ]; then
    echo "missing REF_DUMP: $REF_DUMP" >&2
    exit 2
fi

mkdir -p "$OUT/ours" "$OUT/ref"
fail=0
count=0
for f in "$CORPUS"/*.fbx; do
    [ -f "$f" ] || continue
    name=$(basename "$f")
    if "$OUR_DUMP" "$f" > "$OUT/ours/$name.txt" 2> "$OUT/ours/$name.err" && \
       "$REF_DUMP" "$f" > "$OUT/ref/$name.txt" 2> "$OUT/ref/$name.err"; then
        count=$((count + 1))
    else
        fail=$((fail + 1))
    fi
done

if [ "$fail" -ne 0 ]; then
    echo "differential completed with parser failures: $fail"
    exit 1
fi

echo "differential external OK: $count files parsed by both dumpers"
