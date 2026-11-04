#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
CC=${CC:-cc}
SOURCES=$(tr '\n' ' ' < SOURCES_C89.txt)
$CC -std=c89 -pedantic -Wall -Wextra -Werror -O2 -Iinclude $SOURCES tests/test_phase13_bridge.c -o /tmp/gs3d_test_phase13_bridge
/tmp/gs3d_test_phase13_bridge
$CC -std=c89 -pedantic -Wall -Wextra -Werror -O2 -Iinclude $SOURCES tests/test_phase12.c -o /tmp/gs3d_test_phase12_rerun
/tmp/gs3d_test_phase12_rerun
if [ "${GS3D_RUN_SANITIZER:-0}" = 1 ]; then
  $CC -std=c89 -pedantic -Wall -Wextra -Werror -g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude $SOURCES tests/test_phase13_bridge.c -o /tmp/gs3d_test_phase13_asan
  /tmp/gs3d_test_phase13_asan
fi
