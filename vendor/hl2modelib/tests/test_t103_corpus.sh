#!/bin/sh
# Optional real-asset regression; point to extracted t103 directory.
set -eu
ROOT="${1:?usage: sh tests/test_t103_corpus.sh PATH_TO_EXTRACTED_T103 [OUTPUT_DIR]}"
OUT="${2:-/tmp/hl2m_t103_corpus}"
mkdir -p "$OUT"
find "$ROOT" -type f -name '*.mdl' | while IFS= read -r mdl; do
    stem="${mdl%.mdl}"
    [ -f "$stem.vvd" ] || continue
    [ -f "$stem.dx90.vtx" ] || continue
    name="$(basename "$stem")"
    echo "Checking $name"
    ./hl2m_export_obj89 "$mdl" "$stem.vvd" "$stem.dx90.vtx" "$OUT/$name.obj"
done
