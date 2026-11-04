#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUT=${OUT:-"$ROOT_DIR/tests/public_corpus"}
mkdir -p "$OUT"

if ! command -v git >/dev/null 2>&1; then
    echo "git is required for this fetch helper" >&2
    exit 2
fi

fetch_repo() {
    name=$1
    url=$2
    dst="$OUT/$name"
    if [ -d "$dst/.git" ]; then
        echo "already present: $dst"
    else
        echo "fetching: $url"
        git clone --depth 1 "$url" "$dst"
    fi
    find "$dst" -type f \( -name '*.fbx' -o -name '*.FBX' \) -print
}

# Small/public FBX samples and comparison corpora. Always audit each upstream
# license before redistributing files; this helper only fetches them locally.
fetch_repo fbxtest https://github.com/kenshaw/fbxtest.git || true
fetch_repo ladybug_3d_models https://github.com/ladybug-tools/3d-models.git || true
fetch_repo jammo_character https://github.com/mixandjam/Jammo-Character.git || true

cat <<MSG

Done. To validate fetched files:

FBX_CORPUS_DIRS="$OUT/fbxtest:$OUT/ladybug_3d_models:$OUT/jammo_character" \
BUILD_DIR="$ROOT_DIR/build" \
"$ROOT_DIR/tests/run_real_corpus.sh"

MSG
