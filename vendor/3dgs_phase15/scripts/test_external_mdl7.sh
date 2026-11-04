#!/bin/sh
# External licensed corpus files must be supplied by the user.
set -eu
if [ "$#" -ne 1 ]; then
  echo "usage: scripts/test_external_mdl7.sh path/to/sample.mdl" >&2; exit 2
fi
mkdir -p /tmp/gs3d-corpus-check
./gs3d_audit "$1"
./gs3d_export "$1" /tmp/gs3d-corpus-check/preview
ls -lh /tmp/gs3d-corpus-check/preview.obj /tmp/gs3d-corpus-check/preview.mtl
