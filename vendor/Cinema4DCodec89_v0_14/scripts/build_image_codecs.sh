#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
./scripts/build_vendor_library.sh
${CC:-gcc} -std=c89 -pedantic -Wall -Wextra -Werror -O2 -Iinclude \
 tools/c4d89_imgtool.c build/libc4d89.a -o "build/c4d89_imgtool${EXE:-}"
printf 'Image codecs, independent archive and imgtool built\n'
