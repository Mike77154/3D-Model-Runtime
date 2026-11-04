#!/bin/sh
set -eu
sh ./build.sh
python3 tests/test_skinning.py
${CC:-cc} -std=c89 -pedantic-errors -Wall -Wextra -Werror -O2 -Iinclude src/ss89skin.c tests/test_skin_api.c -o bin/test_skin_api
./bin/test_skin_api tests/fixtures/ska_skin/blend.bm tests/fixtures/ska_skin/blend.skl tests/fixtures/ska_skin/bend.ba
