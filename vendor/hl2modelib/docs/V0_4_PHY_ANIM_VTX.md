# v0.4.0 PHY / animation block / VTX material replacement notes

Version: 0.4.0

This pass moves hl2modelib closer to a complete Source model import path for game engines.

## New decode/import coverage

```text
MDL
  + local animation descriptors
  + local sequence descriptors
  + animblock table metadata
  + surfaceprop/mass/contents/keyvalue offsets

ANI
  + IDST-style companion header
  + animblock byte ranges
  + optional external anim-channel scan when paired with decoded MDL metadata

PHY
  + phyheader_t wrapper
  + checksum / solid count
  + compact VPHY solid headers
  + legacy IVP-style solid header summaries
  + opaque tail offset/size for keyvalues or remaining physics bytes

VTX
  + MaterialReplacementListHeader_t
  + MaterialReplacementHeader_t
  + per-LOD material id -> replacement material name

QC
  + collision source side table
  + material replacement side table
```

## Why PHY is conservative

Source `.phy` stores a collision wrapper plus proprietary physics payload. v0.4.0 decodes the stable wrapper and solid headers, checks the MDL checksum relationship, and exposes offsets/sizes so an engine can preserve or hand off opaque VPhysics bytes. It does not yet reconstruct final convex hull vertices or ragdoll constraints from IVP internals.

## Animation block scope

The MDL decoder now exposes enough animation metadata to answer questions like:

```text
How many local animations exist?
Which animations are embedded vs external ANI blocks?
What fps/frame counts do they use?
Which sequences reference animation blend tables?
Where are zero-frame and section tables located?
```

For embedded MDL animation data, hl2modelib scans the chain of `mstudioanim_t` headers and records bone/channel flags. The next deeper layer would decode the raw/compressed rotation and position payloads into sampled fixed-point poses.

## Material replacement flow

```text
MDL mesh material id
    + selected VTX LOD
    + VTX material replacement list
        -> engine draw material_name / replacement_material
```

Use:

```c
hl2m_build_engine_model(&mdl, &vvd, &vtx, lod, &engine);
```

The builder now calls `hl2m_engine_assign_material_names()` internally. For manual lookup:

```c
hl2m_vtx_find_material_replacement(&vtx, lod, material_id, out_name, out_size);
```

## APIs added

```c
void hl2m_clear_phy(hl2m_phy *out_phy);
void hl2m_clear_ani(hl2m_ani *out_ani);

int hl2m_decode_phy(const hl2m_u8 *data, unsigned int size, hl2m_phy *out_phy);
int hl2m_decode_ani(const hl2m_u8 *data, unsigned int size, const hl2m_mdl *mdl, hl2m_ani *out_ani);

int hl2m_vtx_find_material_replacement(const hl2m_vtx *vtx, int lod, int material_id, char *out_name, unsigned int out_size);
int hl2m_engine_assign_material_names(const hl2m_mdl *mdl, const hl2m_vtx *vtx, int lod, hl2m_engine_model *model);
```

## Still not a StudioMDL clone

v0.4.0 is still a decoder/importer foundation. The big remaining layers are:

- full fixed-point animation decompression and sampling
- writing production `.mdl/.vvd/.vtx/.phy/.ani`
- VTA/flex decode and compile
- collision hull extraction from opaque physics payloads
- VTF texture decode and Source shader execution
