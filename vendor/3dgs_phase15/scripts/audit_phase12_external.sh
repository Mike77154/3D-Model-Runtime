#!/bin/sh
# Usage: sh scripts/audit_phase12_external.sh /path/to/unzipped/user/models
set -eu
cd "$(dirname "$0")/.."
if [ $# -ne 1 ]; then echo 'usage: audit_phase12_external.sh model_directory' >&2; exit 2; fi
for f in "$1"/*.mdl "$1"/*.MDL; do
 [ -f "$f" ] || continue
 echo "=== $f"
 ./gs3d_audit "$f"
done
