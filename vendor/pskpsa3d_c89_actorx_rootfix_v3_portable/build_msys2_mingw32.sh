#!/bin/sh
# MSYS2 MinGW32 - build everything in strict C89, then run unit tests.
set -eu
CC="${CC:-gcc}"
AR="${AR:-ar}"
CFLAGS="-std=c89 -Wall -Wextra -Werror -pedantic-errors -O2"
mkdir -p build
"$CC" $CFLAGS -Iinclude -c src/pskpsa3d.c -o build/pskpsa3d.o
"$AR" rcs libpskpsa3d.a build/pskpsa3d.o
"$CC" $CFLAGS -Iinclude tools/pskpsa_info.c libpskpsa3d.a -o pskpsa_info.exe
"$CC" $CFLAGS -Iinclude tools/pskpsa_skin_dump.c libpskpsa3d.a -o pskpsa_skin_dump.exe
"$CC" $CFLAGS -Iinclude tests/test_minimal.c libpskpsa3d.a -o test_minimal.exe
"$CC" $CFLAGS -Iinclude tests/test_numeric_limits.c libpskpsa3d.a -o test_numeric_limits.exe
./test_minimal.exe
./test_numeric_limits.exe
