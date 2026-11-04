#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
SOURCES="$(tr '\n' ' ' < SOURCES_C89.txt)"
FLAGS="-std=c89 -pedantic -Wall -Wextra -Werror -O2 -Iinclude"
python3 tests/make_fixtures.py
${CC:-cc} $FLAGS $SOURCES tests/test_phase09.c -o gs3d_phase09_tests
./gs3d_phase09_tests
${CC:-cc} $FLAGS $SOURCES tests/test_phase09_robust.c -o gs3d_phase09_robust
./gs3d_phase09_robust
${CC:-cc} $FLAGS $SOURCES src/tools/gs3d_blend_main.c -o gs3d_blend
