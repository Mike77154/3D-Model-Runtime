#!/bin/sh
set -eu
TMP="${TMPDIR:-/tmp}/fragformats_r12_$$"
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
mkdir -p "$TMP"
python3 tests/create_mdl7_r12.py "$TMP"
M="$TMP/MDL7_twojoint_hierarchy.mdl"
./fraginspect "$M" "$TMP/hierarchy.obj" "$TMP/hierarchy.tsv" --frame=1 --skin-hierarchy >/dev/null
./fraginspect "$M" "$TMP/pivot.obj" "$TMP/pivot.tsv" --frame=1 --skin-pivot >/dev/null
./fraginspect "$M" "$TMP/base.obj" "$TMP/base.tsv" --frame=1 >/dev/null
./fraginspect "$M" "$TMP/identity.obj" "$TMP/identity.tsv" --frame=0 --skin-hierarchy >/dev/null
./fraginspect "$TMP/MDL7_forward_parent.mdl" "$TMP/forward.obj" "$TMP/forward.tsv" --frame=1 --skin-hierarchy >/dev/null
./fraginspect "$TMP/MDL7_parent_only.mdl" "$TMP/root_only.obj" "$TMP/root_only.tsv" --frame=1 --skin-hierarchy >/dev/null
# The weighted vertex0 moves from (0,0) to (-.25,+.25) after R90 root * R90 arm;
# pivot-only ignores the parent and yields (+.25,+.25).
grep -q '^v -0.2500 0.2500 0.0000$' "$TMP/hierarchy.obj"
grep -q '^v 0.2500 0.2500 0.0000$' "$TMP/pivot.obj"
grep -q '^v 0.0000 0.0000 0.0000$' "$TMP/identity.obj"
grep -q '^v 0.0000 0.0000 0.0000$' "$TMP/base.obj"
grep -q '^v 0.0000 1.0000 0.0000$' "$TMP/root_only.obj"  # vertex1 bound to root
grep -q '^skin_preview_mode[[:space:]]*mdl7_3x4_hierarchy_local_rest_relative_diagnostic$' "$TMP/hierarchy.tsv"
cmp "$TMP/hierarchy.obj" "$TMP/forward.obj"
for tag in duplicate_transform hierarchy_cycle hierarchy_nan; do
 if ./fraginspect "$TMP/MDL7_${tag}.mdl" "$TMP/invalid.obj" "$TMP/invalid.tsv" --frame=1 --skin-hierarchy >/dev/null; then
  echo "ERROR R12 accepted ${tag}" >&2;exit 1
 fi
done
if ./fraginspect "$M" "$TMP/mix.obj" "$TMP/mix.tsv" --frame=1 --skin-hierarchy --skin-pivot >/dev/null 2>&1; then
 echo 'ERROR accepted conflicting modes' >&2; exit 1
fi
printf '%s\n' 'R12 PASS: two-joint inherited rotation, root-only propagation, forward parent, identity, malformed files'
