#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
make
sh tests/run_tests.sh
python3 tests/create_synthetic_arg.py
for ver in 10001 10002; do
 ./fraginspect "tests/synthetic_arg_${ver}.arg" "tests/arg_${ver}.obj" "tests/arg_${ver}.rig.tsv" > "tests/arg_${ver}.txt"
 grep -q 'ARG OK 3 vertices, 1 triangles' "tests/arg_${ver}.txt"
 test "$(grep -c '^v ' tests/arg_${ver}.obj)" -eq 3
 test "$(grep -c '^vt ' tests/arg_${ver}.obj)" -eq 3
 test "$(grep -c '^vn ' tests/arg_${ver}.obj)" -eq 3
 test "$(grep -c '^f ' tests/arg_${ver}.obj)" -eq 1
 test "$(grep -c '^SKIN' tests/arg_${ver}.rig.tsv)" -eq 3
done
for f in bad_index truncated; do
 if ./fraginspect "tests/${f}.arg" >/dev/null 2>&1; then echo "FAIL accepted $f ARG";exit 1;fi
done
# A non-triangle-list DBO no longer dereferences a null OBJ pointer.
python3 tests/create_bad_primitive_dbo.py
./fraginspect tests/nontri_primitive.dbo >/dev/null
if ./fraginspect tests/bad_primitive_index_count.dbo >/dev/null 2>&1; then
 echo 'FAIL accepted bad DBO index count'; exit 1
fi
# Optional original corpus directory, provided by fragMOTION 1.3.2 RAR.
if [ -n "${ARG_CORPUS:-}" ]; then
 for entry in 'anchor 31 31 0 0' 'locked 12 10 0 0' 'Models/jarjar 344 694 25 1' 'Models/snowman 367 678 7 2'; do
  set -- $entry
  file="$ARG_CORPUS/$1.arg"
  out="tests/$(basename "$1").obj"
  trace="tests/$(basename "$1").rig.tsv"
  ./fraginspect "$file" "$out" "$trace" > tests/real_arg_report.txt
  grep -q "ARG OK $2 vertices, $3 triangles, $4 bones, $5 animations" tests/real_arg_report.txt
  test "$(grep -c '^v ' "$out")" -eq "$2"
  test "$(grep -c '^f ' "$out")" -eq "$3"
  test "$(grep -c '^SKIN' "$trace")" -eq "$2"
 done
 echo 'PASS: 4 authentic ARG meshes decoded, OBJ & rig traces produced'
fi
echo 'PASS: R3, strict C89, synthetic ARG v1.1/v1.2, malformed ARG, DBO guard'
