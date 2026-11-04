#!/bin/sh
# Optional real-corpus test; run from project root with MDL/VVD/VTX inputs.
set -eu
if [ "$#" -ne 3 ]; then echo "usage: $0 TyrantPhase1.mdl TyrantPhase1.vvd TyrantPhase1.dx90.vtx"; exit 2; fi
make hl2m_export_animated_obj89
./hl2m_export_animated_obj89 "$1" "$2" "$3" 1 0 test_t103_walk_v107.obj --entity-motion
./hl2m_export_animated_obj89 "$1" "$2" "$3" 1 28 test_t103_walk_v107.obj --entity-motion
./hl2m_export_animated_obj89 "$1" "$2" "$3" 1 56 test_t103_walk_v107.obj --entity-motion
rm -f test_t103_walk_v107.obj test_t103_walk_v107.mtl
echo 'T-103 frames 0,28,56+root motion OK'
