#!/bin/sh
set -eu
make clean
make all CC=gcc CFLAGS='-std=c89 -O2 -Wall -Wextra -Werror -pedantic -Iinclude'
make check CC=gcc CFLAGS='-std=c89 -O2 -Wall -Wextra -Werror -pedantic -Iinclude'
