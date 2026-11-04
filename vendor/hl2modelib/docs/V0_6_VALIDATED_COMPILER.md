# hl2modelib 0.6.0 validated compiler pass

This pass keeps the clean-room C89/no-allocation rule and pushes the v0.5 binary writer toward a safer engine pipeline.

## New public helpers

```c
int hl2m_make_compile_options_from_qc(const hl2m_qc *qc,
                                      hl2m_compile_options *out_options);

int hl2m_validate_compile_inputs(const hl2m_qc *qc,
                                 const hl2m_smd *reference_smd,
                                 const hl2m_smd *animation_smd,
                                 const hl2m_vta *vta,
                                 hl2m_compile_result *out_result);

int hl2m_validate_binary_bundle(const hl2m_u8 *mdl_data, unsigned int mdl_size,
                                const hl2m_u8 *vvd_data, unsigned int vvd_size,
                                const hl2m_u8 *vtx_data, unsigned int vtx_size,
                                const hl2m_u8 *phy_data, unsigned int phy_size,
                                const hl2m_u8 *ani_data, unsigned int ani_size,
                                hl2m_bundle_report *out_report);

int hl2m_vta_write(const hl2m_vta *vta,
                   char *out_text,
                   unsigned int out_size,
                   unsigned int *out_used);

int hl2m_engine_apply_vta_flex(const hl2m_engine_model *base_model,
                               const hl2m_vta *vta,
                               int flex_index,
                               hl2m_fixed weight,
                               hl2m_engine_model *out_model);
```

## QC-derived compile options

`hl2m_make_compile_options_from_qc()` fills:

- `model_name` from `$modelname`
- `body_name` from the first `$body`/bodygroup studio item captured by the QC parser
- `surfaceprop` from `$surfaceprop`
- compiler LODs from `$lod` blocks
- collision mode hints from `$collisionmodel` / `$collisionjoints`

The high-level compiler also consumes those QC-derived defaults when the caller did not provide stronger options.

## Input preflight

`hl2m_validate_compile_inputs()` reports warnings without allocating memory:

- incomplete QC manifest
- animation skeleton node count mismatch
- bad bone references in SMD vertices/weights
- weights that need repair or normalization
- VTA source vertices outside the reference SMD vertex range

The same warning counters are copied into `hl2m_compile_result` when using `hl2m_compile_qc_smd_to_binary()`.

## Bundle validation

`hl2m_validate_binary_bundle()` reads only compact headers and checks:

- MDL/VVD/VTX magic/version fields
- checksum agreement across MDL/VVD/VTX
- PHY/ANI checksum agreement when present
- MDL declared length versus actual emitted byte count

This is designed for tools that want a cheap post-write sanity check without decoding the entire bundle into large structs.

## Flex/VTA runtime bridge

`hl2m_engine_apply_vta_flex()` copies an existing `hl2m_engine_model` and applies one VTA flex frame using a signed 16.16 blend weight. It uses Source vertex IDs when available and falls back to engine vertex order for SMD-built models.

This gives a game engine a direct path for previewing flex targets before a fully byte-exact StudioMDL flex-controller implementation exists.

## PHY cooking update

`HL2M_COLLISION_PER_BONE_AABB` emits one compact VPHY-style solid per SMD bone using conservative per-bone bounds. This is still a proxy cooker, not IVP/VPhysics convex decomposition, but it is a useful step beyond a single whole-model box.
