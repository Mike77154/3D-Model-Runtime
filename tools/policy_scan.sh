#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if grep -RInE '\b(malloc|calloc|realloc|free|float|double)\b' "$ROOT/include" "$ROOT/src" "$ROOT/tests"; then
    echo "policy_scan: forbidden token found" >&2
    exit 1
fi
echo "policy_scan: runtime glue is clean"
