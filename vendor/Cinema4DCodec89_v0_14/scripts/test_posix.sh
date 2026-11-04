#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
./scripts/build_posix.sh
python3 tests/test_native.py
${CC:-gcc} -std=c89 -pedantic -Wall -Wextra -Werror -O2 -Iinclude \
 tests/test_stream.c build/libc4d89.a -o build/test_stream
if [ -n "${C4D89_STREAM_TEST:-}" ]; then build/test_stream "$C4D89_STREAM_TEST"; fi
