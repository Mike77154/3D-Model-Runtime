#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
./scripts/build_image_codecs.sh
${CC:-gcc} -std=c89 -pedantic -Wall -Wextra -Werror -O2 -Iinclude \
 tests/test_image_resource.c build/libc4d89.a -o "build/test_image_resource${EXE:-}"
"./build/test_image_resource${EXE:-}"
python3 tests/test_image_codecs.py
