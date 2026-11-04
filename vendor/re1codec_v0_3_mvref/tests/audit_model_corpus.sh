#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
INSPECT="$ROOT/build/re1codec_inspect"
ENEMY_DIR=${ENEMY_DIR:-}
PLAYER_DIR=${PLAYER_DIR:-}

if [ ! -x "$INSPECT" ]; then
    echo "build/re1codec_inspect not found; run make first" >&2
    exit 2
fi
if [ -z "$ENEMY_DIR" ] || [ -z "$PLAYER_DIR" ]; then
    echo "usage: ENEMY_DIR=/path/ENEMY PLAYER_DIR=/path/PLAYERS ./tests/audit_model_corpus.sh" >&2
    exit 2
fi

pass=0
fail=0
for f in "$ENEMY_DIR"/*.[Ee][Mm][Dd] "$PLAYER_DIR"/*.[Ee][Mm][Ww] "$PLAYER_DIR"/*.[Tt][Mm][Dd]; do
    [ -f "$f" ] || continue
    if "$INSPECT" "$f" >/dev/null 2>&1; then
        pass=$((pass + 1))
    else
        echo "FAIL: $f" >&2
        fail=$((fail + 1))
    fi
done

echo "corpus audit: pass=$pass fail=$fail"
[ "$fail" -eq 0 ]
