# Phase 08 — Gzip, Zstandard, basic skeletal frame playback

## Implementation

- `src/bc_wrapper.c`: Gzip/Zstd sniffing + stream decode to a seekable temporary
  C file. Gzip uses zlib `inflateInit2(MAX_WBITS+16)` with a static custom
  allocator (8 MiB cap). Zstandard uses `ZSTD_initStaticDStream` with a 32 MiB
  static workspace. The decoder output is capped at 512 MiB. Both accept
  concatenated members/frames and reject truncation/format errors.
- Compressed reads require a writable system temp directory. `tmpfile()` is a
  C runtime service and is not guaranteed to avoid internal heap allocations.
- Binary-only third-party static libs `vendor/linux-x86_64` for this release;
  they are separately licensed and not strict C89. The project-owned source
  compiles with `-std=c89 -pedantic -Wall -Wextra -Werror`.
- `src/bc_playback_tsv.c`: reads extracted rest `Bone.arm_mat`, pose channel
  sample values, an independently prepared vertex/influences TSV; evaluates
  armature-space pose matrices and produces **skinned vertex positions**.
- `--play-blend`: calls `--rest-matrices` and `--sample` internally, then does
  the supported basic LBS pose without manual intermediate file creation.
- `--play-obj`: recovers mesh vertices, OBJ topology, per-vertex weights, vertex
  group names, armature bone names, rest matrices, and Action samples **from a
  single `.blend`**, then emits an animated OBJ mesh at the requested frame.
- `--play-sequence`: repeats the same path for a bounded range of frames,
  generating up to 241 consecutive OBJ files; this is an offline frame
  sequence, not a real-time 3D viewport.

## Commands

```
make
./blendc89 --info compressed.blend
./blendc89 --obj compressed.blend mesh.obj
./blendc89 --play-obj character.blend 5 skinned_frame5.obj [action_id]
./blendc89 --play-sequence character.blend 1 24 exported/clip [action_id]
./blendc89 --play-blend character.blend 5 weights_input.tsv vertices_at_frame5.tsv [action_id]
./blendc89 --animate-tsv rest.tsv sampled.tsv vertices.tsv vertices_out.tsv [action_id]
```

The input vertex format of the playback commands is:
`vertex_index x y z influence_count bone_index weight ...`.

These are **mapped bone indices**, not arbitrary Blender vertex-group indices.
For `--play-obj` and `--play-sequence`, mapping is automatic and uses names
from serialized Blender vertex groups. Non-bone groups are excluded.
The TSV-only path requires manual vertex-group mapping. `--play-obj` performs
name-based mapping but does not resolve Object modifier targets automatically. The rest TSV must contain
armature ID zero and unambiguous bone names. Action filtering defaults to ID 0;
ambiguous duplicated transform channels fail.

### Supported channels

- `pose.bones["BoneName"].location` — x/y/z
- `pose.bones["BoneName"].rotation_quaternion` — w/x/y/z
- `pose.bones["BoneName"].scale` — x/y/z
- Parent/child rest-relative composition with rigid rest matrices.
- Quaternions are normalized approximately in Q16; basic LBS uses Q16 weights.

### Unsupported/NOT complete Blender playback

- Blender 1.x/2.0.x legacy file layouts, IPO-driven poses, NLA blending,
  IK/constraints, drivers, FCurve modifiers, missing FCurve interpolation modes,
  Euler/axis-angle rotation, B-Bones, dual-quaternion skinning, modifiers,
  shape keys, mesh/object transforms and multi-object armature targeting.
- Full rigged animated Blender scene validation: no authentic source sample
  containing both a compatible Armature and a compatible Action was available.
  Tests of this full chain use an **independently generated synthetic `.blend`**.
  `--play-obj` preserves existing OBJ faces and UV lists and only moves `v` rows.
  For arbitrary deformed modifiers, topology changes are unsupported.
- This does not render a video or live graphics: it exports skinned vertices
  at a requested sample frame for an external renderer/engine.
- Bounds are static: max 8192 bones, 8 influences per vertex, 2 GiB legacy
  reader file limit, 512 MiB decompressed wrapper cap.

## Genuine and synthetic regression checks

`python tests/test_phase08.py -v`:
- Real Blender 2.79a startup file (internal format version 272) round-tripped
  as Gzip and Zstandard; header version and 842 block count agree.
- Two truncated compressed streams are rejected.
- Synthetic two-bone weighted quaternion rotation+parent displacement.
- Synthetic file with SDNA Bone + Action FPoint curve, decoded via both wrappers
  and rendered at a fractional local animation value.

- `--play-obj` tested with synthetic `.blend` carrying Mesh, MDeformVert,
  bDeformGroup, Armature, Bone, Action, FCurve and FPoint records, including
  compressed Gzip and Zstandard forms. Three-frame OBJ sequence also passes.
- Genuine 2.58, 3.6.3 and 4.5-source sample files successfully parsed after
  wrapping as Gzip and Zstandard, but they are *not* validated rigged animations.
- 160 compressed-stream mutations completed without OS signals; no claim of
  adversarial security from that limited test.

The static archives are for Linux x86-64 only; bundled Windows build uses the
uncompressed-only stub. The file formats and decoded math are not claimed
fully equivalent to Blender's runtime.
