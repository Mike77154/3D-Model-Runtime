#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BAD='malloc|realloc|free[[:space:]]*\(|\bfloat\b|\bdouble\b'
if grep -RniE "$BAD" "$ROOT/include" "$ROOT/src" "$ROOT/tools" "$ROOT/tests" --include='*.c' --include='*.h'; then
    echo "constraint audit FAILED"
    exit 1
fi
echo "constraint audit PASS: no heap allocation API and no floating types"
