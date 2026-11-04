# Phase 09 — Q16.16 Euler animation, pose rotation modes, safe text field parsing

Status: experimental C89 decoder with an expanded but incomplete Blender skeletal animation path. Date: 2026-11-02.

## Exact additions

1. `src/bc_euler_q16.c` converts serialized Euler angles (in radians, Q16.16) to WXYZ quaternions in **all six Blender Euler orders**: XYZ(1), XZY(2), YXZ(3), YZX(4), ZXY(5), ZYX(6). The algorithm uses a bounded integer-only CORDIC sine/cosine approximation and the Blender Euler-order/parity conventions from the user-supplied Blender 2.79a `source/blender/blenlib/intern/math_rotation.c` (the mathematical transformation is newly implemented, not copied). No libm calls in the new module, floats, 64-bit types, or dynamic allocation.
2. `bc_playback_tsv.c` now accepts `pose.bones["Bone"].rotation_euler` components 0–2 and converts them to local rotation quaternions before hierarchical Q16.16 matrix playback and linear blend skinning. Quaternion animation remains supported.
3. `bc_pose.c` appends serialized Euler components and `rotmode` to `--pose` output, when present in the input's SDNA; missing old fields fall back to zero and quaternion mode. Direct `.blend` playback reads those pose-channel modes and rejects incompatible quaternion/Euler combinations or unsupported axis-angle mode.
4. Generated TSV parsing separates fields by **tabs** whenever tabs are present, preserving bone names and RNA paths with spaces, but still accepts older whitespace-delimited test input. Blender-style `\\"` and `\\\\` escapes in RNA bone names are decoded rather than rejected.
5. Explicitly rejects Euler and quaternion channels for one bone in the same sampled Action; mixed transformations could otherwise create ambiguous, silently wrong animation.

## API and commands

The existing CLI remains intact, including compressed-file input through the Phase08 Linux-static wrapper:

```sh
make clean all
./blendc89 --pose model.blend pose_defaults.tsv
./blendc89 --sample model.blend keyframe5.tsv 5
./blendc89 --play-blend model.blend 5 mapped_vertices.tsv skinned.tsv [action_id]
./blendc89 --play-obj model.blend 5 frame5.obj [action_id]
./blendc89 --play-sequence model.blend 1 24 clip [action_id]
python3 tests/test_phase09_euler.py
```

For `--animate-tsv`, where the `.blend`'s `bPoseChannel.rotmode` is not supplied, Euler curves default **explicitly to XYZ**; this makes basic interchange possible but is not a claim of correctly identifying other rotation orders without a `.blend` pose channel. Real `.blend` playback uses the serialized mode (if available).

## Validation performed (Linux x86-64)

- C89: `cc -std=c89 -pedantic -Wall -Wextra -Werror -O2 -Iinclude -Ivendor/headers` succeeded.
- 282 Euler conversion cases, including large positive and negative angles, six rotation orders and independently calculated Python reference values. Maximum absolute quaternion component error: **0.0001847**, normalized Q16 representation.
- An XYZ Z-axis 90° rotation moved a weighted `(1,0,0)` vertex to approximately `(0,1,0)`.
- TSV input containing a bone named `Upper Arm.L`, plus an escaped quote in a bone name, exported correctly.
- Synthetic 64-bit-pointer `.blend` with a real SDNA `bPoseChannel.rotmode=ZYX`, three Action FCurves and one armature bone passed an end-to-end `--play-blend` check against an independently calculated reference point.
- All previously supplied regression scripts 01–08 passed in this environment, including Phase07 export of seven authentic static `.blend` files and Phase08 Gzip/Zstd tests. **This does not validate animation equivalence for genuine rigged `.blend` assets.**

## Outstanding work, no false completion claim

- Blender's dependency graph, bone constraints, IK solvers, B-Bone segments, drivers, NLA and layered Action blending, multi-armature object targeting, object transform application, shape keys, mesh modifiers and dual-quaternion preserve-volume skinning are not implemented.
- Current play commands export offline OBJ frames/TSV, not realtime on-screen clip playback or audio/video.
- The BSD/zlib-licensed Linux x86-64 precompiled libraries in `vendor/linux-x86_64` remain third-party binaries. **The entire Gzip/Zstandard libraries have not been rewritten to strict C89**, and native 32-bit Windows compressed input remains unsupported by the supplied stub build.
- The new CORDIC kernel is approximate; it does not promise bitwise-equivalent Blender rotations, and very large input values can be rejected by Q16.16 bounds.
- Files larger than the original reader's limit or compressed wrapper's size cap remain unsupported.

## License

All newly authored source in Phase09 is independent code under the existing project LICENSE. Consult the third-party notices under `vendor/` for zlib and Zstandard. No Blender GPL source, authored textures, or assets are redistributed.
