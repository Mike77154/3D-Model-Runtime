#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
./scripts/build_posix.sh
python tests/test_native_v04.py
