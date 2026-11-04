#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
./scripts/build_posix.sh
${CC:-gcc} -std=c89 -pedantic -Wall -Wextra -Werror -O2 -Iinclude \
 tests/test_fixed.c build/libc4d89.a -o build/test_fixed
./build/test_fixed
python3 tests/test_directmesh.py
