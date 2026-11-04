#!/usr/bin/env sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $0 /path/to/ENEMY" >&2
    exit 2
fi

ROOT=$1
TOOL=${TOOL:-./build/re1codec_mv_audit}
PASS=0
FAIL=0

for f in "$ROOT"/*.[Ee][Mm][Dd]; do
    if "$TOOL" "$f"; then
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
    fi
done

echo "SUMMARY pass=$PASS fail=$FAIL"
[ "$FAIL" -eq 0 ]
