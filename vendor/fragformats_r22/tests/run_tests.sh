#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
make
python3 tests/create_synthetic_dbo.py
python3 tests/create_dbo_invalid_index.py
python3 tests/create_synthetic_xir.py
printf "BAD_DBO" > tests/malformed.dbo
./fraginspect tests/synthetic_plane.dbo tests/synthetic_plane.obj > tests/report_dbo.txt
grep -q 'DBO OK' tests/report_dbo.txt
test "$(grep -c '^v ' tests/synthetic_plane.obj)" -eq 4
test "$(grep -c '^vn ' tests/synthetic_plane.obj)" -eq 4
test "$(grep -c '^vt ' tests/synthetic_plane.obj)" -eq 4
test "$(grep -c '^f ' tests/synthetic_plane.obj)" -eq 2
./fraginspect tests/synthetic_mesh.xir tests/synthetic_mesh.obj > tests/report_xir.txt
grep -q 'XIR OK' tests/report_xir.txt
test "$(grep -c '^v ' tests/synthetic_mesh.obj)" -eq 4
test "$(grep -c '^vn ' tests/synthetic_mesh.obj)" -eq 4
test "$(grep -c '^vt ' tests/synthetic_mesh.obj)" -eq 4
test "$(grep -c '^f ' tests/synthetic_mesh.obj)" -eq 2
for path in tests/malformed.dbo tests/bad_index_plane.dbo tests/bad_index_mesh.xir tests/truncated_mesh.xir tests/bad_count_mesh.xir; do
 if ./fraginspect "$path" >/dev/null 2>&1; then echo "ERROR: accepted malformed $path"; exit 1; fi
done
echo 'PASS: DBO synthetic + XIR synthetic, 5 malformed inputs (authentic ARG tested in R3)'
