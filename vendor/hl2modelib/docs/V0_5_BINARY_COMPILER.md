# hl2modelib v0.5.0 - binary compiler path

This pass adds the first end-to-end binary writer path:

```text
QC + reference SMD + optional animation SMD + optional VTA
    -> MDL + VVD + VTX + PHY + ANI
    -> decode again
    -> build fixed-point engine model
```

The implementation is still clean-room and conservative. It does not copy Valve SDK code and does not try to be a byte-for-byte `studiomdl` replacement. The goal is a portable game-engine codec layer that can emit internally consistent Source-style binary pieces without dynamic allocation.

## Added API

```c
void hl2m_clear_compile_options(hl2m_compile_options *out_options);
void hl2m_clear_compile_result(hl2m_compile_result *out_result);

int hl2m_decode_vta(const hl2m_u8 *data, unsigned int size, hl2m_vta *out_vta);

int hl2m_write_mdl_from_qc_smd(...);
int hl2m_write_vvd_from_smd(...);
int hl2m_write_vtx_from_smd(...);
int hl2m_write_phy_from_smd(...);
int hl2m_write_ani_from_smd(...);

int hl2m_compile_qc_smd_to_binary(...);
```

## Binary output summary

### MDL writer

Emits an IDST v48 style header using the existing decoder-compatible layout:

- model name/checksum
- bounds and hitbox set
- bones from SMD nodes
- textures from SMD materials
- cdmaterials path from QC
- one bodypart/model with per-material meshes
- local animation descriptor and sequence descriptor when an animation SMD is supplied
- animblock table pointing at the generated ANI companion data layout
- keyvalue breadcrumb when VTA flex data is supplied

### VVD writer

Emits vertex payload:

- header/checksum/LOD vertex counts
- bone weights and bone ids
- position, normal, UV
- generated tangents

The writer keeps all numeric payloads as integer-to-IEEE32 bit conversion from fixed point; no C float/double operations are used.

### VTX writer

Emits draw topology:

- bodypart/model/LOD/mesh/stripgroup hierarchy
- triangle-list strips
- per-material meshes
- per-LOD triangle-step simplification
- material replacement lists from QC `renamematerial` / `replacematerial`

### PHY writer

Emits a conservative one-solid VPHY wrapper with an AABB-derived compact surface summary and KeyValues tail. This is collision cooking for broad engine use, not a complete IVP/VPhysics convex cooking solver.

### ANI writer

Emits an IDST-style ANI companion with:

- two animblock entries: block 0 reserved, block 1 data
- scan-compatible channel headers per bone
- appended `HL2A` fixed-point baked pose payload for engine-side tooling

### VTA parser

Parses Source-style vertex animation frame data into fixed-capacity flex records and vertex deltas.

## Regression path

`examples/hl2m_compile_probe.c` performs the whole round trip:

```sh
./hl2m_compile_probe \
  examples/synthetic/test.qc \
  examples/synthetic/test.smd \
  examples/synthetic/test_anim.smd \
  examples/synthetic/test.vta
```

Expected shape:

```text
compile=0 mdl=... vvd=... vtx=... phy=... ani=... warnings=...
decoded bones=1 mats=1 meshes=1 lods=2 vtx_indices=6 phy_solids=1 ani_blocks=2 ani_channels=1 engine_vertices=3 draws=1
```

## Honest scope

The writer path is now complete enough to generate parseable `MDL/VVD/VTX/PHY/ANI` bundles from `QC+SMD+VTA` for simple/static/rigged test meshes. Still not complete:

- not a byte-exact StudioMDL clone
- not a full VPhysics convex decomposition/cooking implementation
- no production flex rule VM yet
- no DMX, VTF, procedural/jiggle/IK solving writer yet
- no full activity/event compilation beyond metadata capture

The important win is architectural: the library now has a real binary output side, not just decoders.
