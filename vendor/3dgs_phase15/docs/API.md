# 3DGS C89 modular vendor API

## Memory ownership / thread model

All public storage is static (`gs3d_model`, `gs3d_rgba`, `gs3d_palette`).
There is no heap allocation, runtime thread safety or reentrancy. Open only one
model at a time; `gs3d_close()` closes owned files but does not free caller memory.
The backing memory or `GS3D_Source` callback userdata **must remain valid** until close.

## Opening a model

```c
#include "gs3d.h"
int rc = gs3d_open("model.mdl");
if (rc == GS3D_OK) {
    /* Inspect gs3d_model.group[0] ... */
    gs3d_close();
}
```

Five per-generation entry points are implemented in separate files
(`src/vendors/gs3d_a4.c` to `gs3d_a8.c`). They are **format filters** rather than
five duplicate MDL parsers. An A8 model's actual signature might be MDL5 or MDL7.

## Memory buffer: no temporary files

```c
/* Caller owns `bytes` for the entire open session. */
int rc = gs3d_open_memory(bytes, byte_count);
if (rc == GS3D_OK) {
    gs3d_apply_frame(0U,0U);
    gs3d_close();
}
```

## Virtual files inside archives (callback source)

```c
#include "gs3d.h"
/* read_at must return 0 on success */
int my_reader(void *user,unsigned long off,unsigned char *dst,unsigned long n);
void example(void *archive,unsigned long model_size){
    GS3D_Source source;
    source.read_at=my_reader;
    source.userdata=archive;
    source.size=model_size;
    if(gs3d_open_source(&source)==GS3D_OK)gs3d_close();
}
```

The callback reads offsets relative to the start of the model, not the container.
The opening operation performs bounds checks before invoking the callback.

## MDL7 matrix keys

```c
GS3D_BoneKey key;
if(gs3d_model.format==7 && gs3d_model.group[0].frames[0].trans_count) {
    if(gs3d_read_bone_key(0U,0U,0UL,&key)==GS3D_OK) {
        /* key.bone; key.matrix[0..15]: raw file order, Q16.16 */
    }
}
```

This returns key data; it deliberately does not assume a world/local matrix
composition convention or implement weighted skinning.

## Format registry

`gs3d_detect_magic()` checks the first four bytes. `gs3d_family_supports()`
selects profiles A4..A8. `gs3d_format_info()` and
`gs3d_format_by_signature()` expose the static registry.
`GS3D_FormatInfo.flags` indicates data categories in the reader, **not**
production-complete rendering, shader support or full skeletal animation.

## Capacities

Use `include/gs3d_config.h` and build *every translation unit* with matching
capacity macros. No runtime allocation is performed. Large capacity values
increase the global model's `.bss` footprint. On 32-bit platforms, ensure
adequate address space.

## Export diagnostics

`gs3d_export_obj(prefix)` emits `.obj`, `.mtl`, and one TGA per decoded skin
with name `<prefix>_group<N>_skin<M>.tga`. Unsupported skin types are not
decoded; the geometry is still exported. For final rendering, use the model
arrays / indices and a native GPU pipeline instead of OBJ.

## Optional selective builds

Each family can be compiled without the other with the macros
`GS3D_WITH_MDL7=0` or `GS3D_WITH_LEGACY=0`. Choose the matching
source manifest (`SOURCES_LEGACY_ONLY.txt` / `SOURCES_MDL7_ONLY.txt`).
For example (Unix / MSYS2 shell):

```sh
cc -std=c89 -pedantic -Iinclude -DGS3D_WITH_MDL7=0 \
   $(tr '\n' ' ' < SOURCES_LEGACY_ONLY.txt) src/tools/gs3d_cli.c -o legacy_probe
```

`SOURCES_READER_ONLY.txt` excludes exporters but keeps both import families.
Build macros must be consistent across all source files. When compiled out,
opening the corresponding format returns `GS3D_ERR_UNSUPPORTED`.

## Phase 03: rigid bone pose (MDL7 only)

```c
int gs3d_build_pose(unsigned int group, unsigned int frame);
int gs3d_apply_pose(unsigned int group, unsigned int frame);
int gs3d_transform_point(unsigned int bone, const GS3D_Vec *rest, GS3D_Vec *out);
int gs3d_find_frame(unsigned int group, const char *exact_name);
extern GS3D_Mat gs3d_skin_matrix[GS3D_MAX_BONES];
```

Call `gs3d_build_pose` before reading the skin matrices or calling `gs3d_transform_point`. `gs3d_apply_pose` builds a pose, restores the original base model vertices and applies the skinned coordinates to the selected group; repeated calls with the same frame are idempotent. The matrices apply only rigid single-bone attachments (no weighted deformers). Group 0 supplies shared keys, consistent with the historical MDL7 importer convention. See `docs/ANIMATION_PHASE03.md` for assumptions.
