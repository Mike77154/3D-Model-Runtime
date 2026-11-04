# hl2modelib QC + engine pipeline notes

Version: 0.3.0

This pass extends hl2modelib beyond isolated file decoders and keeps shaping it into a game-engine-oriented pipeline:

```text
QC/SMD authoring side         compiled Source side              engine side
---------------------        -----------------------           -----------------------------
.qc  build recipe      --->   .mdl metadata / skeleton  --->    hl2m_engine_model draw batches
.smd skeleton/mesh            .vvd vertex payload                fixed-point vertices
.vmt material KVs             .vtx topology/strip groups         triangle-list output
```

## Main APIs

```c
void hl2m_clear_qc(hl2m_qc *out_qc);
void hl2m_clear_engine_model(hl2m_engine_model *out_model);
void hl2m_clear_material_desc(hl2m_material_desc *out_desc);

int hl2m_decode_qc(const hl2m_u8 *data, unsigned int size, hl2m_qc *out_qc);
int hl2m_qc_write(const hl2m_qc *qc, char *out_text, unsigned int out_size, unsigned int *out_used);
int hl2m_decompile_mdl_to_qc(const hl2m_mdl *mdl, char *out_text, unsigned int out_size, unsigned int *out_used);
int hl2m_make_basic_qc(const hl2m_smd *smd, const char *modelname, const char *body_name, const char *smd_path, const char *cdmaterials, int staticprop, hl2m_qc *out_qc);

int hl2m_smd_write(const hl2m_smd *smd, char *out_text, unsigned int out_size, unsigned int *out_used);

int hl2m_build_engine_model(const hl2m_mdl *mdl, const hl2m_vvd *vvd, const hl2m_vtx *vtx, int lod, hl2m_engine_model *out_model);
int hl2m_build_engine_model_from_smd(const hl2m_smd *smd, hl2m_engine_model *out_model);
int hl2m_engine_model_bounds(const hl2m_engine_model *model, hl2m_vec3 *out_min, hl2m_vec3 *out_max);
int hl2m_engine_model_validate(const hl2m_engine_model *model);
int hl2m_engine_model_write_obj(const hl2m_engine_model *model, const char *object_name, char *out_text, unsigned int out_size, unsigned int *out_used);

int hl2m_vmt_find_param(const hl2m_vmt *vmt, const char *key, char *out_value, unsigned int out_size);
int hl2m_vmt_to_material_desc(const hl2m_vmt *vmt, hl2m_material_desc *out_desc);
```

## QC parser scope

The parser is fixed-buffer and no-heap. It handles:

- comments: `//` and `/* ... */`
- quotes
- braces and block depth
- top-level `$commands`
- bodygroup inner commands like `studio` and `blank`
- LOD inner commands like `replacemodel`
- common compile/decompile commands such as `$modelname`, `$body`, `$model`, `$bodygroup`, `$cdmaterials`, `$sequence`, `$animation`, `$collisionmodel`, `$collisionjoints`, `$staticprop`, `$surfaceprop`, `$contents`, `$scale`, `$origin`, `$rotate`, `$texturegroup`, `$keyvalues`, `$includemodel`, `$attachment`, `$bonemerge`, `$hitboxset`, `$hbox`, `$definebone`, `$lod`, `$renamematerial`, `$eyeposition`, `$illumposition`, `$poseparameter`, `$weightlist`, `$ikchain`, `$ikrule`, `$bbox`, `$cbox`, `$mostlyopaque`, `$opaque`, `$translucent`, `$ambientboost`, `$maxverts`, `$maxbones`, `$jigglebone`, `$proceduralbones`, `$alwayscollapse`, `$collapsebones`, `$keepbone`, `$skipboneinbbox`, `$texrendermode`, `$jointmerge`.

Structured side tables now capture:

- `hl2m_qc_body`
- `hl2m_qc_sequence`
- `hl2m_qc_lod`
- `hl2m_qc_attachment`
- `hl2m_qc_hitbox`
- `hl2m_qc_definebone`
- `hl2m_qc_poseparameter`

This is still an IR/manifest layer, not a full StudioMDL-equivalent compiler.

## Engine model builder scope

`hl2m_build_engine_model()` validates `MDL/VVD/VTX` checksum sync and produces a render-oriented fixed-buffer `hl2m_engine_model`:

- fixed-point positions, normals, UVs, tangents
- bone IDs/weights
- material IDs from MDL mesh metadata
- triangle-list output indices
- draw batches carrying bodypart/model/mesh/lod provenance

`hl2m_build_engine_model_from_smd()` does the same for raw SMD triangles, giving a direct import path for tools or engines that want to use the source mesh without compiled Source assets.

## Material bridge

`hl2m_vmt_to_material_desc()` extracts common renderer-binding values:

- shader class
- `$basetexture`
- `$bumpmap`
- `$surfaceprop`
- `$detail`
- `$envmap`
- flags: translucent, alphatest, nocull, selfillum, additive, vertexlit, unlit, lightmapped

It does not execute Source shader logic or VTF decoding.

## Examples

```sh
make
make check
./hl2m_probe examples/synthetic/test.qc
./hl2m_probe examples/synthetic/test.vmt
./hl2m_smd_engine_probe examples/synthetic/test.smd
./hl2m_bundle_probe examples/synthetic/test.mdl examples/synthetic/test.vvd examples/synthetic/test.vtx
```

## Design limits preserved

- C89
- no dynamic allocation ownership
- no C floating-point types or operations in library code
- Source IEEE32 values are converted by bit-decoding into 16.16 fixed point
