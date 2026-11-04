# hl2modelib 1.0.4 — Vendored Source module layout + SDK root motion

## Why the previous Walk_F looked unusual

In Valve's `public/bone_setup.cpp` (`Studio_AnimPosition`, `CalcBoneQuaternion`,
`CalcBonePosition`), rendering a native MDL animation is not just selecting a
bone pose per integer frame: it can involve **movement curves**, blending and
inverse kinematics. The previous OBJ exporter read bone poses but discarded
movement curves. The T-103 corpus demonstrably has a **movement record** in
`Walk_F` for all three monster phases. The SDK's movement lives on the
**entity transform**, not on the mesh's local joints. Blending and IK are
still not implemented for native MDL, so this is not complete animation
parity. The legacy renderer also frames each OBJ independently, hiding
world-space travel.

The SDK also says `STUDIO_ANIM_DELTA` without raw position/rotation
uses the zero position / identity quaternion. 1.0.4 corrects that case and
adds a dedicated regression. Important: `Walk_F` in the T-103 phase-1 model
has channel flags `0x2C` and **not DELTA**; therefore the direct improvement
for its walk cycle is **new root-motion extraction**, not the delta patch.

## Folders and ownership

```
include/
  hl2modelib.h                   Existing ABI, all pre-existing functions kept
  hl2m_master89.h                New facade / master codec entry points
src/
  hl2modelib.c                   Legacy shared support and aggregate-format TU
vendors/source/
  master/hl2m_master89.c         Public dispatcher and convenience pipelines
  formats/mdl_codec89.inc        Source48/49 MDL Studio metadata
  formats/vvd_codec89.inc        VVD vertices, skinning weights, UVs, tangents
  formats/vtx_codec89.inc        VTX strips, index maps, LOD draw tables
  formats/vmt_codec89.inc        VMT KeyValues and surface material descriptors
  formats/phy_codec89.inc        PHY collision/solid metadata
  formats/ani_codec89.inc        ANI block metadata
  mesher/hl2m_mesher89.c       Independently linked mesher public adapter
  mesher/hl2m_mesher_impl89.inc  Source GPU strip/tri decoding to engine draws
  mesher/hl2m_mesher_build89.inc Full MDL + VVD + VTX mesh assembler
  animator/hl2m_native_anim89.c Native bone decoder, bind matrices and movement
  animator/hl2m_skinner_impl89.inc CPU matrix palette skinning
  texture/hl2m_vtf89.c          VTF 7.x BC1/BC3 (DXT1/DXT5) to RGBA8
  texturizer/hl2m_shader89.c    VMT->FX and fixed-point software shading
```

The `.inc` files are *physically separated source implementations* but are
compiled within the legacy `src/hl2modelib.c` translation unit. This is
necessary for the original internal `static` helpers and maintains source
compatibility without duplicating the parsers. They are NOT separately
linkable `.o` libraries. The mesher adapter, VTF, animator, shader and master modules are
**independently compiled `.c` translation units**, linked together into the
static `libhl2modelib.a`. No SDK files are included in the distribution.

The other original `.smd`, `.vta`, `.qc` parsers are retained in legacy core
and accessible through the master decoder; they have not yet been physically
split out. Do not represent this release as a full breakup of every helper.

## API usage

Use `#include "hl2m_master89.h"` and link with `libhl2modelib.a`.

* `hl2m_master_guess89(path, data, size)` determines a Source subformat.
* `hl2m_master_decode89(format,data,size,mdl,lod,out)` dispatches MDL, VVD,
  VTX, VMT, VTF metadata, ANI, PHY, SMD, VTA, QC. `out` MUST point to the
  corresponding `hl2m_*` struct; ANI requires an already decoded MDL.
* `hl2m_master_mesh89(mdl,vvd,vtx,lod,out)` assembles a drawable mesh.
* `hl2m_master_anim89(data,size,mdl,index,frame,pose,report)` samples local
  animation into hierarchical world-space matrices.
* `hl2m_native_inverse_bind89(...)` remains the inverse-bind matrix builder.
* `hl2m_master_skin89(base,inverse,pose,out,report)` skins the mesh.
* `hl2m_master_motion89(data,size,mdl,index,frame,&position,&yaw)` samples
  entity movement for one frame, not joint motion; the presenter should place
  the animated entity at that world transform.
* `hl2m_master_texture89(vtf_data,size,rgba,capacity,&info)` decodes a VTF.
* `hl2m_master_material89(vmt_data,size,&vmt,&fx)` interprets the material and
  shader parameters. A separate presenter supplies pixel values and normals
  for `hl2m_shade_fragment89`.

The caller owns all large structs, image buffers and byte arrays. For Win32
prefer static allocation (`static hl2m_mdl mdl;`) rather than stack placement.
For corrupt 'lost-first-byte' T-103 files, call
`hl2m_repair_missing_first_byte()` **on a writable caller-owned copy** before
passing bytes to the master facade.

## CLI and root motion

```
make CFLAGS='-std=c89 -Wall -Wextra -Werror -pedantic -Iinclude'
make check CFLAGS='-std=c89 -Wall -Wextra -Werror -pedantic -Iinclude'
./hl2m_anim_sdk_audit89 /path/to/tyrantphase1.mdl
./hl2m_motion_probe89 /path/to/tyrantphase1.mdl
./hl2m_export_animated_obj89 phase1.mdl phase1.vvd phase1.dx90.vtx 1 28 pose.obj
./hl2m_export_animated_obj89 phase1.mdl phase1.vvd phase1.dx90.vtx 1 28 pose_moving.obj --entity-motion
```

The optional `--entity-motion` adds Source movement position to the exported
OBJ as a **debug convenience**. Root **yaw is reported but is not applied**
in the OBJ exporter (rotation pivot would need its own verified mapping).
For an engine, keep the base skinned mesh in local space and separately
apply the entity's position/yaw as the runtime transform.

## Verified T-103 files

The entire supplied seven MDL/VVD/VTX sets reconstructed as OBJ without
missing geometry errors. Source 48 and Source 49 were both exercised.

Native walk movement measured from the model's movement curve:

| Corpus | Walk_F frames | Motion along X at final frame |
|---|---:|---:|
| Tyrant phase 1 | 57 | ~47.507 Source units |
| Tyrant phase 2 | 130 | ~169.434 Source units |
| Tyrant phase 3 | 255 | ~382.465 Source units |

At phase-1 frame 28, `--entity-motion` shifts OBJ X coordinates by
approximately +23.753 units relative to the in-place pose. No bone topology,
material assignment or texture mapping is changed by this flag.

## Deliberate limitations

1. Native MDL samples are still **integer-frame poses**, no Source-equivalent
   fractional frame, pose blending, gesture layers, procedural IK or foot lock.
   Foot sliding / stiff movement can still occur.
2. Externally stored animation blocks (`.ani`) and compound animation
   sections do not have complete native pose sampling.
3. `VTF` RGBA decoder supports only tested DXT1/DXT5 BC1/BC3, one frame,
   2D textures. Unsupported formats return errors instead of fake colors.
4. Shader FX is a C89 fixed-point approximation, not the Valve GPU engine.
5. Aggregate `.inc` vendor code requires legacy internal support and cannot
   be linked individually. Use the public master API or static library.
6. Built and tested under host GCC; MSYS2 MINGW32 cross-build was not run in
   this session.

## Reference

Valve SDK 2013, `public/bone_setup.cpp`: `CalcBoneQuaternion`,
`CalcBonePosition`, `Studio_AnimPosition`.
Valve SDK 2013 `public/studio.h`: `mstudioanim_t`, `mstudiomovement_t`,
`mstudioanimdesc_t`.
Valve SDK 2013 `public/mathlib/compressed_vector.h`: `Quaternion48/64`.
