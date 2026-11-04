# Source SDK 2013 findings used for hl2modelib

The uploaded SDK zip was inspected and the library was written from scratch instead of copying SDK code.

## Files inspected

```text
src/public/studio.h
src/public/optimize.h
src/utils/smdlexp/smdlexp.cpp
src/public/tier1/KeyValues.h
src/tier1/KeyValues.cpp
game/mod_hl2mp/materials/example_model_material.vmt
```

## `.mdl`

`studio.h` defines the Studio Model version used by Source SDK 2013 as `STUDIO_VERSION 48`. The primary header is `studiohdr_t` and acts as the offset table for the rest of the file. The clean-room decoder reads:

- magic `IDST`
- version/checksum/name/length
- eye/illum/hull/view bounds
- flags
- `mstudiobone_t` array
- `mstudiotexture_t` array
- cdtexture path table
- `mstudiobodyparts_t` -> `mstudiomodel_t` -> `mstudiomesh_t`
- `mstudiohitboxset_t` -> `mstudiobbox_t`

## `.vvd`

`studio.h` also defines `vertexFileHeader_t`, `vertexFileFixup_t`, `mstudioboneweight_t`, and `mstudiovertex_t`.

The decoder reads:

- magic `IDSV`
- version 4
- checksum matching `.mdl`
- LOD vertex counts
- fixup table
- fat vertices: weights, bone IDs, position, normal, texcoord
- optional tangent block

## `.vtx`

`optimize.h` defines `OptimizedModel` structures under `#pragma pack(1)`, version 7. The decoder follows the packed nested offsets:

```text
FileHeader_t
  BodyPartHeader_t
    ModelHeader_t
      ModelLODHeader_t
        MeshHeader_t
          StripGroupHeader_t
            Vertex_t[]
            unsigned short index[]
            StripHeader_t[]
```

`ModelLODHeader_t::switchPoint` is decoded to 16.16 fixed point through the same integer-only conversion path.

## `.smd`

`smdlexp.cpp` prints the classic SMD ASCII sections:

```text
version 1
nodes
  id "name" parent
end
skeleton
  time n
  bone px py pz rx ry rz
end
triangles
  material
  parent px py pz nx ny nz u v [links...]
  parent px py pz nx ny nz u v [links...]
  parent px py pz nx ny nz u v [links...]
end
```

The decoder stores nodes, skeleton rows, materials, triangles, vertices, and up to 3 weights per vertex.

## `.vmt`

VMT files use Source KeyValues syntax. The decoder implements a compact scanner for quoted/unquoted tokens, braces, and comments. It stores shader name plus flattened key/value entries with depth so nested blocks such as `Proxies` survive.

## QC / StudioMDL recipe layer

This pass adds a clean-room QC command scanner rather than a full StudioMDL clone. QC is treated as a build recipe and asset manifest: `$modelname`, `$cdmaterials`, `$body`, `$model`, `$bodygroup`/`studio`, `$sequence`, collision commands, `$keyvalues`, `$lod`, attachments and hitbox commands become fixed-buffer IR records. This IR can later feed a stricter compiler, a game-engine importer, or a decompiler/manifest writer.

## Engine-friendly assembly

The new `hl2m_build_engine_model()` joins decoded MDL, VVD and VTX data into an engine-facing fixed-buffer mesh:

```text
MDL bodypart/model/mesh material + vertex offset
VTX stripgroup/strip/index/origMeshVertID remap
VVD vertex payload: position, normal, uv, tangent, weights
```

It verifies the common checksum first. It then walks bodypart -> model -> requested LOD -> mesh -> stripgroup -> strip and emits fixed-point triangle-list buffers plus draw batches.

This is deliberately conservative: it does not execute Source shaders, rebuild flexes, compile VTF textures, or reproduce every StudioMDL optimization yet.

## v0.3.0 additions

v0.3.0 keeps the same clean-room boundary and adds engine-oriented helper layers around the decoded data:

- direct SMD -> engine model import
- SMD text writer
- QC generator for basic static/reference model manifests
- structured QC capture for LODs, attachments, hitboxes, definebones and poseparameters
- compact VMT material descriptor extraction
- debug OBJ writing from `hl2m_engine_model`

These helpers are deliberately built around caller-provided buffers and fixed-capacity structs.


## v0.4.0 findings

Additional SDK headers inspected for this pass:

```text
src/public/phyfile.h
src/public/optimize.h
src/public/studio.h
src/common/studiobyteswap.cpp
```

### `.phy`

`phyfile.h` exposes `phyheader_t` with four integer fields: wrapper size, id, solid count and checksum. `studiobyteswap.cpp` shows the next payload as one solid header per VPhysics solid. Modern compact solids begin with a `VPHY` id and include version, model type, surface byte size, drag axis areas and axis-map size. Older legacy solids use a different IVP-style header. hl2modelib v0.4.0 decodes these headers and preserves the remaining bytes as opaque physics/keyvalue payload; it does not attempt to rebuild the proprietary convex/IVP surface data yet.

### MDL animation and sequence metadata

`studio.h` stores `mstudioanimblock_t`, `mstudioanimdesc_t` and `mstudioseqdesc_t` behind offsets in `studiohdr_t`. v0.4.0 captures the fixed metadata needed by an engine scheduler/decompiler: anim names, fps, frame counts, animblock/index routing, section info, zero-frame info, sequence labels, activity names, sequence bboxes, blend counts and blend parameters. Embedded animation channel chains are scanned conservatively through the small `mstudioanim_t` headers. Full Source animation decompression is left as a later layer.

### VTX material replacement lists

`optimize.h` places a material replacement list table in `FileHeader_t`. Each LOD can hold a `MaterialReplacementListHeader_t`, and each entry maps a material id to a replacement material name. v0.4.0 decodes this table and the engine bridge can resolve draw material names through the replacement table for the selected LOD.
