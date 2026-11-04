#!/bin/sh
# Build each independent C89 archive, then assemble old API into libc4d89.a.
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
for mod in core mesher uv_texturer shaderer animer texturer_opener; do
    ./scripts/build_module.sh "$mod"
done
rm -f build/libc4d89.a build/libc4d89_images.a
${AR:-ar} rcs build/libc4d89.a build/obj/*/c4d89_*.o
${AR:-ar} rcs build/libc4d89_images.a build/obj/texturer_opener/c4d89_img_*.o
printf 'Built combined libc4d89.a + compatible image-only libc4d89_images.a\n'
