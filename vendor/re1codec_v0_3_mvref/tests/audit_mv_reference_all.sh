#!/usr/bin/env sh
set -eu

if [ "$#" -ne 2 ]; then
    echo "usage: $0 /path/to/ENEMY /path/to/PLAYERS" >&2
    exit 2
fi

ENEMY=$1
PLAYERS=$2
TOOL=${TOOL:-./build/re1codec_mv_audit}
PASS=0
FAIL=0

run_one() {
    if "$TOOL" "$1"; then
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
    fi
}

for f in "$ENEMY"/*.[Ee][Mm][Dd]; do run_one "$f"; done
for f in "$PLAYERS"/*.[Ee][Mm][Ww]; do run_one "$f"; done
for f in "$PLAYERS"/*.[Tt][Mm][Dd]; do run_one "$f"; done

echo "SUMMARY pass=$PASS fail=$FAIL"
[ "$FAIL" -eq 0 ]
