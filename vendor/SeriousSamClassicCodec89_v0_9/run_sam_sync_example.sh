#!/bin/sh
# User-supplied game assets must be extracted first.
# Usage: sh run_sam_sync_example.sh PATH_TO_SeriousSam.amc ASSET_ROOT OUTDIR
set -eu
if [ "$#" -ne 3 ]; then echo "usage: sh $0 scene.amc assets outdir"; exit 2; fi
AMC="$1"; ASSETS="$2"; OUT="$3"
mkdir -p "$OUT"
./bin/ss89scene "$AMC" --root "$ASSETS" --sequence "$OUT/SamSync" --count 23 --step-ms 27 --root-clip RUN --body-clip NORMALWALK --sync-body
n=0
while [ "$n" -lt 23 ]; do
  t=$(printf '%04d' "$n")
  ./bin/ss89present "$OUT/SamSync_$t.obj" "$OUT/frame_$t.ppm" 145
  n=$((n+1))
done
printf '%s\n' 'Frames exported; optional: ffmpeg -framerate 24 -i "$OUT/frame_%04d.ppm" Sam.mp4'
