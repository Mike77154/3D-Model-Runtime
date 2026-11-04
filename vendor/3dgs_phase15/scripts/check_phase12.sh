#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
python3 tests/make_fixtures.py
python3 tests/make_phase12_fixtures.py
CC=${CC:-cc}
SOURCES=$(tr '\n' ' ' < SOURCES_C89.txt)
$CC -std=c89 -pedantic -Wall -Wextra -Werror -O2 -Iinclude $SOURCES tests/test_phase12.c -o gs3d_phase12_tests
./gs3d_phase12_tests
$CC -std=c89 -pedantic -Wall -Wextra -Werror -O2 -Iinclude $SOURCES tests/test_deform_weights.c -o gs3d_phase04_regression
./gs3d_phase04_regression
$CC -std=c89 -pedantic -Wall -Wextra -Werror -O2 -Iinclude $SOURCES src/tools/gs3d_audit_main.c -o gs3d_audit
$CC -std=c89 -pedantic -Wall -Wextra -Werror -O2 -Iinclude $SOURCES src/tools/gs3d_export_main.c -o gs3d_export
