#!/bin/sh
# Compile exactly one independent module/archive without rebuilding the others.
# Example: ./scripts/build_module.sh animer
set -eu
cd "$(dirname "$0")/.."
. ./scripts/module_sources.sh
module=${1:-}
case "$module" in
    core)            sources=$C4D89_CORE_SOURCES ;;
    animer)          sources=$C4D89_ANIMER_SOURCES ;;
    shaderer)        sources=$C4D89_SHADERER_SOURCES ;;
    mesher)          sources=$C4D89_MESHER_SOURCES ;;
    uv_texturer)     sources=$C4D89_UV_TEXTURER_SOURCES ;;
    texturer_opener) sources=$C4D89_TEXTURER_OPENER_SOURCES ;;
    *) echo "Usage: $0 core|animer|shaderer|mesher|uv_texturer|texturer_opener" >&2; exit 2 ;;
esac
CC=${CC:-gcc}
AR=${AR:-ar}
FLAGS="-std=c89 -pedantic -Wall -Wextra -Werror -O2 -Iinclude -Isrc"
mkdir -p build/obj
rm -rf "build/obj/$module"
mkdir -p "build/obj/$module"
objects=""
for src in $sources; do
    base=${src##*/}
    obj="build/obj/$module/${base%.c}.o"
    "$CC" $FLAGS -c "$src" -o "$obj"
    objects="$objects $obj"
done
rm -f "build/libc4d89_$module.a"
$AR rcs "build/libc4d89_$module.a" $objects
printf 'Built module %s (%s files)\n' "$module" "$(set -- $sources; echo $#)"
