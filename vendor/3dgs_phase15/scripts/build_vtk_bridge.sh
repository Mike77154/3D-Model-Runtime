#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
: "${CC:=cc}"
SOURCES=$(tr '\n' ' ' < SOURCES_C89.txt)
$CC -std=c89 -pedantic -Wall -Wextra -Werror -O2 -fPIC -shared -Iinclude $SOURCES -o libgs3d_vtk.so
