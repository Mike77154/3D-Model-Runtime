# Runtime 75 — RE4 Classic PMD/FCV animated character bridge (C89)

**Scope:** new implementation in `src/providers/provider_re4classic89.c`, built on the
original RE4 v4.1 `re4_skel_evaluate`, `re4_skel_skin_vertex`,
`re4_skel_skin_rigid_node`, `re4_character_link`, `re4_character_audit`, and
`re4_character_retarget`. Keeps RE3 V028, Gun Survivor V19, HOD2 Phase12, IQM.
No game model, texture, motion clip or executable is included.

## What is genuinely connected

1. Open a **master `.pmd`**, with `modelrt_open_file()`; probe favors RE4 if it is
   not an MMD PMD. Native base-pose triangles/materials/UV/normals continue to work.
2. Open a **separate `.fcv`** in a different handle; FCV stays native and independent.
3. Initialize a `modelrt_animation_binding` with caller-owned scratch and call
   `modelrt_animation_bind(&binding, &pmd, &fcv)`.
   The match uses strict bone identities recognized by the vendor codec. At least
   one verified rotation track (bone type 2) must map to exactly one PMD joint.
   Ambiguous, zero-match, duplicated named rotation tracks are rejected.
4. Call `modelrt_re4_visit_fcv_frame(&binding, native_tick, &sink, user)` to emit
   skinned triangles, updated animated bone world translations and existing UVs.
   Both 2-weight inverse-bind and rigid node delta branches are supported.
   A PMD with no skeleton remains accessible as static geometry.
5. For extra heads/hands/clothing meshes, open **each explicit accessory PMD**
   independently, then call `modelrt_re4_visit_character_frame()` with part handles,
   native frame, scratch, callbacks. This uses strict, audited master/part
   retargeting; it does NOT guess which PMDs belong to a character.

`modelrt_visit_bound_animation(binding,0,q16_native_frame,...)` is also supported
for native-frame coordinates that fit Q16.16. This does **not** mean seconds:
FCV has no proven FPS. For the complete 0..65535 native-tick range, use
`modelrt_re4_visit_fcv_frame()` instead. This is a provider-specific convention.

## Multipart texture/material routing

The composite visitor sends one model begin/end pair and remaps each piece's
mesh indices, material indices and bone indices into disjoint ranges. Call:

```c
modelrt_handle *part_owner;
modelrt_u32 local_texture;
modelrt_re4_character_texture_source(&master, parts, part_count,
    global_material_texture_index, &part_owner, &local_texture);
modelrt_texture_info_get(part_owner, (int)local_texture, &texture);
```

Each part's texture paths remain owned by its original handle. There is no
inferred texture atlas or material identity across parts. The caller must keep
all handles alive until the visitor completes. A rejected part is not silently
welded to the root.

## Example in C89

```c
/* Initialize runtime and handles, then open master.pmd and motion.fcv. */
modelrt_animation_binding_init(&binding, binding_buffer, binding_buffer_size);
if (modelrt_animation_bind(&binding, &master, &motion) == MODELRT_OK) {
    modelrt_re4_visit_fcv_frame(&binding, 5u, &sink, user);
    /* Optional: an explicitly selected list of accessory PMDs. */
    modelrt_re4_visit_character_frame(&binding, parts, part_count, 5u,
        part_scratch, part_scratch_size, &sink, user);
}
```

## Real-world inspector, source-only

```
make re4-character-inspect
./examples/re4_character_inspect89 master.pmd motion.fcv 5 [part1.pmd ...]
```

This visitor prints native model/motion geometry counts and fixed-point bounds
for **user-supplied** files. It is a diagnostic console inspector, not a GUI
rasterizer. Its default static buffers handle a 32 MiB PMD per part and a
16 MiB FCV; adjust those compile-time limits before testing larger assets.

## Synthetic regression — no proprietary files

```
make re4classic-animated-synthetic
make re4classic-synthetic
```

The generated one-bone/three-vertex PMD and one rotation FCV exercise:
- bind-pose triangle at `(65536,0,0)` (Q16);
- rotation/skin at frame 5, returning approximately `(57513,31419,0)`;
- full character render of 2 explicitly supplied PMD instances, unique
  mesh/material indices, and texture/material index routing;
- invalid tick and binding lifetime checks.

The build also ran RE3 V028, Gun Survivor V19, HOD2 Phase12 and IQM
regressions under GCC/Linux. Win32/MinGW32 and authentic Capcom model/FCV
pairs were **not** tested here.

## Limitations

- Native IK targets/constraints, root displacement channels, game-driven
  special-role FCV data, animation event scheduling and FPS are not inferred.
- Valid structural part retargeting is **not** canonical identity verification.
  Bella variants, head mutations, weapons and Plaga overlays must be supplied
  explicitly and visually audited with genuine resources.
- Material alpha blend ordering, UV/textures on every asset, bone hierarchy
  mistakes specific to game files, and non-PC PMD versions remain unverified.
- Strict C89 integration uses no heap or floating point. It does not replace
  the source codec with a universal model representation.
