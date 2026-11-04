# MilkShape 3D binary v4 / 1.8.5 — investigation (v1.6)

## Primary references

- Mete Ciragan format header, expanded annotations by sapper-trle (2015): https://gist.github.com/sapper-trle/db1dc6670ec0fa733d7d
- Paul Bourke, MS3D 1.3/1.4 format baseline: https://paulbourke.org/dataformats/ms3d/index.html
- MM3D compatibility release notes (vertex-extra v3, keyframes, weights): https://github.com/clover-moe/mm3d/releases
- Additional local references supplied by the user: `ms3d.c`, `ms3dloader-master.zip`, `milkshape_plugins-master.zip`, `UnityExtraAnimations-master.zip`, `imbalance-master.zip` (not bundled due to unrelated source licensing).

## Binary layout

All multi-byte integers and IEEE-754 float32 bit patterns are little-endian.
The runtime decodes float32 **bits** to Q16.16 integers (it does not perform floating-point operations).

| Section | Bytes per element / details |
| --- | --- |
| Header | magic `MS3D000000` (10) + 32-bit version = 4 (4) |
| Vertices | count u16; each 15 bytes: flags u8, xyz[3] float32, primary bone i8, reference count u8 |
| Triangles | count u16; each 70 bytes: flags u16, vertex indices[3] u16, corner normals[3][3] float32, s[3]/t[3] float32, smoothingGroup/groupIndex u8 each |
| Groups | count u16; each has flags u8, name[32], triangleCount u16, variable triangle-index list, materialIndex i8 |
| Materials | count u16; each 361 bytes: name, RGBA ambient/diffuse/specular/emissive, shininess, transparency, mode, texture path[128], alpha path[128] |
| Animation header | FPS float32, current time float32, total frames i32, joint count u16 |
| Joints | flags, name[32], parentName[32], reference rot xyz and local position, independent rotation/translation key lists; each key is time float32 + xyz float32 (16 bytes) |
| Comments | optional 32-bit subversion=1; four comment categories; **empty** comment lists are valid |
| Vertex extra | optional 32-bit subversion 1/2/3 + per-vertex boneIds[3] and weights[3], plus 0, 4 or 8 bytes application payload |
| Joint extra | optional subversion 1 + RGB color float32[3] for each joint |
| Model extra | optional subversion 1 + joint size float32, transparency mode i32, alphaRef float32 |

## What changed in v1.6

1. **Corrected vertex-extra subversion 1 weight denominator.** The specification explicitly defines 0..100 byte percentages in all supported versions; the old implementation assumed 255 for subversion 1. The residual is `max(0,100 - (w0+w1+w2))`, assigned to fourth bone. See `tests/test_ms3d_quat.c` for a synthetic four-influence regression. If an old export intentionally uses a nonstandard 255 scale, that is a distinct incompatible dialect, not silently auto-detected.
2. Added independent `gif_ms3d_quat.c` / `.h`: quaternions from authored local Euler XYZ rotations, normalized shortest-hemisphere linear interpolation, matrix conversion. **NLERP, not exact SLERP.**
3. `ms3d_skin_evaluate_mode` can use `MS3D_ROTATION_EULER` (legacy) or `MS3D_ROTATION_QUAT_NLERP` (new). The old `ms3d_skin_evaluate` API still means Euler; `ms3d_player_prepare` defaults to quaternion NLERP. `ms3d_player_rotation_mode` selects an alternative.
4. New `ms3d_spec_audit` command checks optional subversions, whole-file consumption, reversed keyframe intervals and invalid group hints.
5. Existing UV/material frame callbacks are maintained unchanged. A `zmb_hs` 4-frame OBJ/MTL/textured render is included as a smoke test.

## Motion semantics

- Joints have parentName links. Reference transforms use extrinsic XYZ rotations, represented as `Rz*Ry*Rx` on column vectors.
- Animation keys contain **delta** Euler radians / local translation offsets, relative to reference joints, and independent time stamps in seconds.
- `global_animated = global_parent * reference_local * key_local`; `skinning = global_animated * inverse(global_bind)`.
- Poses use linear blend skinning over as many as four stored influences. Normals are direction-transformed and normalized.
- Q16.16 samples introduce precision limits, particularly very long timelines and extreme scales. Millisecond playback is bounded by its present fixed-point time representation.
- MS3D does **not** define a standard, named multi-clip catalog. A game-specific `idle/walk/attack` clip table must be authored independently, using frame/time ranges. The player already exposes range, loop, once and seek.

## UV and material mapping

- `s[3]` and `t[3]` are three pairs per triangle: **corner-level UVs**. Corner normals must likewise remain split to preserve hard edges.
- For direct rendering keep native MS3D UV coordinates; convert to OBJ with `vt=(s,1-t)` **exactly once**. The model callback returns native values.
- Material membership follows the *group triangle-index lists*; triangle.groupIndex is only a fallback hint. Never assume one texture for every triangle or that group indices are contiguous in triangle order.
- The `texture[128]` / `alphamap[128]` fields are external file paths, NOT embedded compressed images. Decoding JPG/BMP/TGA/PNG/DDS is a renderer/asset-loader concern.
- Transparency has its own model-level mode (simple / alpha-reference / depth-sorted). Importing these fields does not implement the corresponding GPU blend/depth state.

## Regression evidence

157 Night of the Zombies files: 157 successful parses and 157 successful three-frame playback traces (0/500/1000 ms). Per-file auditing confirms no trailing bytes and no decreasing key times.

| Total | Value |
| --- | ---: |
| Models | 157 |
| Vertices | 102,832 |
| Triangles | 109,751 |
| Materials | 535 |
| Joint records | 151 |
| Vertices with nonzero extended weights | 41,796 |
| Vertex extra subversion 2 | 133 models |
| No vertex extra | 24 models |
| Vertex extra subversion 1 or 3 | **0 corpus models** (synthetic v1 test only) |
| Trailing bytes after parse | 0 |

Additional independent test: `dwarf.ms3d` from UnityExtraAnimations (46 joints, 7074 rotation keys + 7074 translation keys, 1794 triangles) parsed and produced three frame traces with stable triangle count, UV and material identifier for the sampled corner.

## Remaining work

- Reference parity against MilkShape app or another known-good animation renderer (joint transform order / nlerp vs true SLERP / scale checks). The corpus tests are structural, **not visual equivalence proof**.
- Additional synthetic file-format tests for vertex-extra subversion 3 and malformed optional blocks.
- Explicit named clip manifest and clip blending / transitions.
- TGA/BMP/PNG/DDS/JPG decoding within a native engine and correct alpha-reference handling.
- Mesh boundary inspection/patching for `zmb`/`zmb_hs` only after distinguishing author seams from true holes.
- Native Win32/MinGW32 compiler execution and visual GPU renderer integration still unverified here.
