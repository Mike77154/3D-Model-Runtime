#!/bin/sh
# Offline verifier: user supplies files obtained from the upstream project.
# No third-party corpus is redistributed in the source package.
set -eu
if [ "$#" -ne 3 ]; then
  echo 'usage: scripts/verify_public_corpus.sh MDL5_acid.mdl MDL7_acid.mdl MDL7_sphere.mdl' >&2
  exit 2
fi
if [ ! -x ./gs3d_audit ]; then
  echo 'Run sh BUILD_LINUX_TEST.sh first.' >&2
  exit 2
fi
assert_contains() {
  model=$1; expected=$2
  case "$model" in *'|'*) exit 2;; esac
  output=$(./gs3d_audit "$model") || return 1
  printf '%s\n' "$output" | grep -F "$expected" >/dev/null || {
    printf 'Unexpected corpus metadata for %s; expected %s\n' "$model" "$expected" >&2
    return 1
  }
  printf 'PASS %s: %s\n' "$model" "$expected"
}
assert_contains "$1" 'format=MDL5 size=15612 groups=1 bones=0 verts=496 triangles=960'
assert_contains "$2" 'format=MDL7 size=28944 groups=8 bones=0 verts=496 triangles=960'
assert_contains "$3" 'format=MDL7 size=3720 groups=1 bones=0 verts=62 triangles=120'
