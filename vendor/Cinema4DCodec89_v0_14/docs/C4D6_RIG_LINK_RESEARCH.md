# Native XC4DC4D6 rig/animation evidence — 2026-10-10

This document deliberately distinguishes **what was observed**, **what was inferred**, and **what is still unknown**. This is not Maxon's serialization specification.

## Native source evidence

In actual user-provided `L_walk.c4d`, the authored object name is followed by a typed UTF-16BE `Joint` class label before key track labels. For example:

| Object name | Name offset | Joint type offset | First associated track |
| --- | ---: | ---: | --- |
| L_walk | 133059 | 133210 | `Position . X` at 133595 |
| spine | 158571 | 158720 | `Rotation . H` at 159105 |
| R_arm1 | 182512 | 182663 | `Rotation . H` at 183048 |
| R_arm4 | 231211 | 231362 | `Position . X` at 232965 |
| L_arm1 | 279502 | 279653 | `Rotation . H` at 280038 |

The v0.5 track scanner retains the most recent **name immediately preceding** a typed `Joint` marker if separated by at most 500 bytes; each following scalar track is assigned that **owner candidate** until the next typed `Joint` marker. This is stronger evidence than grouping tracks by their order, but it is not a decoded binary object link.

In five selected animation files, all 888 scalar tracks have a nearby `Joint`/owner candidate. This does NOT imply the scene graph's object ID, parent-child relationship or rig skinning is decoded. See `ANIMATION_OWNER_AUDIT.csv` for offsets, names and counts.

### Matrices

`FemaleHobo.c4d` includes 33 name-adjacent typed `Joint` records. The scanner finds 32 plausible `0x19` big-endian float32 4×3 arrays following those type markers. These have the C4D-like 12-value column order `(offset.xyz, v1.xyz, v2.xyz, v3.xyz)`. **The 32 recovered arrays are identities in this sample**. They should NOT be passed as inverse-bind or skin matrices: actual mesh deformations require a joint rest state and a complete parent hierarchy.

The 33rd name-adjacent marker has no validated matrix; as such the inventory can include non-joint container artifacts. The term `record` is preferable to claiming all 33 entries are skeleton bones.

### Weight tag

The native `Weight` token and its associated payloads are detectable. But the stored per-point index/weight schema, the joint list, and rest-state object references have **not** been established by a reproducible parse. The v0.5 code does not guess the counts from arbitrary bytes or treat 0x80-block values as vertex indices. Native weight reads remain unsupported.

Maxon's published SDK explains the *meaning* of a weight tag: an indexed joint list and per-point weights, and explicitly warns that the sparse weight order does not match point order. This is not a public on-disk layout. Reference: https://developers.maxon.net/docs/cpp/2025_2_0a/page_manual_caweigthtag.html and https://developers.maxon.net/docs/cpp/2026_1_0/classcinema_1_1_c_a_weight_tag.html . **No SDK code or binary is bundled**.

## Implemented v0.5 source modules

- `c4d89_anim.c`: owner candidate, owner name/joint offsets, spatial-evidence flag, unchanged key scalar decoder.
- `c4d89_joint_records.c`: 0x82 typed joint record inventory and candidate 0x19 matrix in Q16.16. No parent hierarchy or weight interpretation.
- `c4d89_skin_core.c`: independent, checked 32-bit Q16.16 CPU skinning for at most 4 *supplied and normalized* influences per vertex. Input joint matrices **must already equal** `animated_global * inverse_bind`. No runtime allocations or floating-point source types. This is a portable component for later integration, not native weight reading.
- `tools/c4d89_inspect.c`: new `nodes` and `trackowners` inspection commands.
- `tests/test_skin_core.c`: synthetic identity, rotation, translation/blending and malformed-input tests.

## Next reverse-engineering gate

1. Decode genuine object parent references and inspect non-identity local/global matrices with nested sample scenes.
2. Decode native CAWeightTag joint references and sparse per-point data with `vertex_count` correspondence. Enforce weight range and normalized sums per vertex.
3. Establish native inverse-bind/rest states, not generic object matrices.
4. Only then route v0.5's Q16.16 CPU kernel into the real rig with `animated_global * inverse_bind`.
5. Cross-check posed mesh against a trusted Cinema 4D render, including animation axes and H/P/B rotations.

Do not claim a native animated zombie preview until all 1–5 are complete.
