#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
python3 tests/create_arg_pose_r18.py
./fraginspect tests/r18_pose.arg tests/r18_pose.obj tests/r18_pose.tsv --arg-clip=0 --arg-frame=5 > tests/r18_pose.log
# 0.5 Q16 Euler X, 5.0 Q16 translation X, 1.5 Q16 scale X/Y/Z.
grep -q '^POSE_BONE_Q16[[:space:]]1[[:space:]]-1[[:space:]]0[[:space:]]5[[:space:]]32768[[:space:]]0[[:space:]]0[[:space:]]327680[[:space:]]0[[:space:]]0[[:space:]]98304[[:space:]]98304[[:space:]]98304' tests/r18_pose.tsv
[ "$(grep -c '^POSE_CURVE_Q16' tests/r18_pose.tsv)" -eq 3 ]
[ "$(grep -c '^ROTATE' tests/r18_pose.tsv)" -eq 2 ]
[ "$(grep -c '^TRANSLATE' tests/r18_pose.tsv)" -eq 2 ]
[ "$(grep -c '^SCALE' tests/r18_pose.tsv)" -eq 2 ]
if ./fraginspect tests/r18_pose.arg tests/reject.obj tests/reject.tsv --arg-clip=0 --arg-frame=11 >/dev/null 2>&1;then echo 'FAIL: accepted frame outside clip';exit 1;fi
if ./fraginspect tests/r18_pose.arg tests/reject.obj tests/reject.tsv --arg-clip=1 --arg-frame=5 >/dev/null 2>&1;then echo 'FAIL: accepted absent clip';exit 1;fi
if ./fraginspect tests/r18_bad_bone.arg >/dev/null 2>&1;then echo 'FAIL: accepted undefined bone ID';exit 1;fi
if ./fraginspect tests/r18_pose.arg tests/reject.obj tests/reject.tsv --arg-clip=0 >/dev/null 2>&1;then echo 'FAIL: accepted incomplete selector';exit 1;fi
if ./fraginspect tests/r18_pose.arg tests/reject.obj tests/reject.tsv --arg-clip=0 --arg-frame=5 --arg-frame=6 >/dev/null 2>&1;then echo 'FAIL: accepted duplicate selector';exit 1;fi
./fraginspect tests/r18_pose.arg tests/r18_pose_start.obj tests/r18_pose_start.tsv --arg-clip=0 --arg-frame=0 >/dev/null
./fraginspect tests/r18_pose.arg tests/r18_pose_end.obj tests/r18_pose_end.tsv --arg-clip=0 --arg-frame=10 >/dev/null
grep -q '^POSE_BONE_Q16[[:space:]]1[[:space:]]-1[[:space:]]0[[:space:]]0[[:space:]]0[[:space:]]0[[:space:]]0[[:space:]]0' tests/r18_pose_start.tsv
grep -q '^POSE_BONE_Q16[[:space:]]1[[:space:]]-1[[:space:]]0[[:space:]]10[[:space:]]65536[[:space:]]0[[:space:]]0[[:space:]]655360' tests/r18_pose_end.tsv
# Real corpus validation is optional for independent source-only package.
if [ -f "${ARG_SNOWMAN_REAL:-/nonexistent}" ];then
 ./fraginspect "$ARG_SNOWMAN_REAL" tests/r18_snowman.obj tests/r18_snowman.tsv --arg-clip=1 --arg-frame=7 >tests/r18_snowman.log
 [ "$(grep -c '^POSE_BONE_Q16' tests/r18_snowman.tsv)" -eq 7 ]
fi
echo 'PASS R18: ARG curve sampling, frame/clip validation, undefined bone rejection, key preservation'
