#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
make
${CC:-cc} -std=c89 -pedantic-errors -Wall -Wextra -Werror -Iinclude \
 tests/integration/actorx_serious_bridge.c libmodel_runtime89.a -o tests/integration/actorx_serious_bridge
./tests/integration/actorx_serious_bridge \
 vendor/SeriousSamClassicCodec89_v0_9/tests/fixtures/v009.mdl \
 tests/integration/fixtures/actorx_triangle.psk \
 tests/integration/fixtures/actorx_triangle.psa
