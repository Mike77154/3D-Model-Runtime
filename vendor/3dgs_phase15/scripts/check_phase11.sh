#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
SOURCES="$(tr '\n' ' ' < SOURCES_C89.txt)"
FLAGS="-std=c89 -pedantic -Wall -Wextra -Werror -O2 -Iinclude"
python3 tests/make_fixtures.py >/dev/null
${CC:-cc} $FLAGS $SOURCES tests/test_phase11.c -o gs3d_phase11_tests
./gs3d_phase11_tests
${CC:-cc} $FLAGS $SOURCES tests/test_phase11_robust.c -o gs3d_phase11_robust
./gs3d_phase11_robust
${CC:-cc} $FLAGS $SOURCES src/tools/gs3d_additive_main.c -o gs3d_additive
