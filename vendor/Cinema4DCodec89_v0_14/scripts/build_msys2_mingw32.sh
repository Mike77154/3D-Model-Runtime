#!/bin/sh
# Execute from MSYS2 MINGW32 with CC=gcc (or i686-w64-mingw32-gcc).
set -eu
cd "$(dirname "$0")/.."
EXE=.exe ./scripts/build_posix.sh
