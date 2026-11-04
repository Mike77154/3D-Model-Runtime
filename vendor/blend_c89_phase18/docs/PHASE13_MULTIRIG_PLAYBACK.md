# Phase 13 — Independent per-rig skeletal playback

## Actual change

This release extends Phase 12 without replacing its earlier decoder/animation modules.

The new `--play-rigs file.blend FRAME PREFIX` command identifies each eligible mesh object that has exactly one supported Armature modifier, resolves its Mesh datablock and armature Object via the *serialized pointers*, and evaluates the armature's own AnimData/Action or narrowly supported single contributing NLA strip. The result is **one mesh-local OBJ per eligible mesh**, named `PREFIX_meshNNNN.obj`, where NNNN denotes the serialized Mesh ID-block index.

A separate `--rig-pairs file.blend map.tsv` command emits a read-only reference map of mesh ID, armature data ID, armature Object index and the stored local Q16.16 matrices. `--rig-timeline`, `--objects` and all previous commands remain available.

### Isolation invariants

1. Mesh/armature associations are determined by `ArmatureModifierData.object` and `Object.data`, **not names** and not the order of Actions in the file.
2. Bone rest matrices are read **only** from the selected armature ID. The selected armature's `bPoseChannel` metadata is filtered by its Object index. Thus same-named bones in different rigs do not collide.
3. Vertex-group weights are read **only** from the selected Mesh ID. The emitted OBJ has vertex and UV indices starting at 1 per mesh.
4. Action/slot selection comes from the correct armature Object (`Object.adt`); one supported non-overlapping NLA strip, scale/repeat timing and muted tracks use the Phase 12 time mapper.
5. The mesh-local/armature-local conversion is performed separately per pair. Parent/constraint graphs, other modifiers and unsupported Armature modifier options are rejected, not approximated.
6. The command attempts to roll back generated OBJ outputs from the invocation when a later rig fails. Existing files with those same names may be overwritten; use a fresh output prefix.

## Tested with generated binary fixtures

The Phase 13 fixture contains two *distinct* bArmature datablocks, two bone records **both named `Root`**, two distinct Actions/FCurves, two Mesh datablocks with independent deform weight arrays, two Armature modifiers and four Objects. The mappers are deliberately **cross-linked** (mesh 0 to armature 1, mesh 1 to armature 0). The resulting first vertex values at scene frame 5 are:

- Mesh 0: `v 1.0000 -3.0000 0.0000` (Action 1, Y translation)
- Mesh 1: `v 5.0000 0.0000 0.0000` (Action 0, X translation)

The regression checks 32-bit/64-bit serialized pointers in both little and big endian; a mixed scene with **one NLA retimed strip and one direct Action**; and refusal of a missing target or unsupported modifier. Tests also validate that the corresponding per-mesh OBJ faces are indexed from 1.

Existing regressions for IPO/FCurves, fixed-point arithmetic, rigging, Euler, object transforms, gzip/zstd decoding, NLA timing and seven **genuine static `.blend` mesh exports** remain intact.

**Not yet tested:** Frame-by-frame numerical agreement of a genuine Blender-authored two-character animation with Blender's evaluated dependency graph. No user-uploaded true dual-character rig is included in the regression corpus.

## Usage

```sh
make clean all
./blendc89 --rig-pairs scene.blend links.tsv
./blendc89 --rig-timeline scene.blend 5 timeline.tsv
./blendc89 --play-rigs scene.blend 5 frame
# frame_mesh0000.obj, frame_mesh0001.obj ...
python3 tests/test_phase13_multirig.py
```

## Conservative boundaries

- Only a normal, single **vertex-group LBS** Armature modifier per mesh; unsupported modifiers/flags and unresolved target references cause refusal.
- Each Mesh datablock can only be used by one eligible mesh Object in this command. Instancing with multiple Object transforms is currently rejected.
- No parenting or object/bone constraints, IK, drivers, B-Bones, preserve-volume dual-quaternion skinning, shape keys, nonlinear/evaluated meshes, NLA mixing of overlapping strips, or arbitrary multi-slot/Action mixing.
- Output meshes are in **their own mesh-local coordinate frames**; this is not Blender's evaluated world-space render or a merged multi-character OBJ.
- Scene elements without a supported Armature modifier are not exported by `--play-rigs` (use `--obj` for static geometry).
- Capacity: 256 matched mesh/armature bindings, 8192 bones per selected rig, 250,000 vertices per selected mesh, 8 weight influences per vertex. Unsupported/oversize inputs are rejected.
- Our project-authored implementation remains strict C89 (32-bit ints; no explicit heap or floating-point types). The **precompiled static zlib/Zstandard dependencies are NOT C89/heapless** and Linux x86-64 specific. MinGW32 compressed support remains unverified.
- `tmpfile` and standard C library I/O can themselves allocate storage internally.

This release adds **multi-rig independent playback**, not a complete Blender NLA mixer or full dependency-graph evaluator. Blender GPL source archives provided by the user served as structural research references; original Blender source code has not been redistributed in the codec ZIP.
