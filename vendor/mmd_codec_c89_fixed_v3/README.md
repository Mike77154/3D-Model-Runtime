# mmd_codec_c89_fixed_v3

C89 / no-heap / fixed-point importer for MikuMikuDance assets.

## Supported scope

| Asset | Loader | Coverage |
|---|---|---|
| `.pmd` | `mmd_load_model()` / `mmd_load()` | Mesh, indices, materials, PMD texture/sphere path extraction, bones, IK, face morphs, display records, English blocks, toon names, optional rigid bodies and joints when present. |
| `.pmx` | `mmd_load_model()` / `mmd_load()` | PMX 2.0/2.1 header, UTF-8/UTF-16LE text, local/English names/comments, vertices, extra UVs, BDEF1/BDEF2/BDEF4/SDEF/QDEF, variable index widths, textures, materials, bones, IK, morph offsets, display frames, rigid bodies, joints, and PMX 2.1 soft bodies. |
| `.vmd` | `mmd_load_vmd()` | Bone keys, morph keys, camera keys, light keys, self-shadow key count/skip. |
| `.vpd` | `mmd_load_vpd()` | Permissive pose parser for common MMD VPD text exports, including `Bone0{name` inline blocks, comments, count validation, translation, and quaternion rotation. |

## v3 upgrades

This pass closes the three v2 gaps:

1. **PMX soft bodies are now exposed as structs**
   - `MMD_SoftBody soft_bodies[MMD_MAX_SOFT_BODIES]`
   - `MMD_SoftBodyAnchor soft_body_anchors[MMD_MAX_SOFT_BODY_ANCHORS]`
   - `MMD_SoftBodyPin soft_body_pins[MMD_MAX_SOFT_BODY_PINS]`
   - stores shape, material, group mask, flags, B-link distance, clusters, mass, collision margin, aero model, Bullet-style config values, cluster values, solver iteration counts, material stiffness values, anchors, and pins.

2. **PMX UTF-16LE Japanese text is transcoded to UTF-8**
   - BMP CJK/kana/kanji survives into fixed char buffers.
   - UTF-16 surrogate pairs are handled.
   - Invalid Unicode raises `MMD_FLAG_INVALID_UNICODE` and emits `?` for the bad unit.
   - Truncated output raises `MMD_FLAG_TRUNCATED_TEXT` while the stream still advances correctly.

3. **VPD parser is less brittle**
   - Accepts `Bone0{bone_name` inline syntax.
   - Still accepts the older split style used by v2 tests.
   - Ignores `//` line comments.
   - Reads `.osm;` model line into `pose->model_name`.
   - Reads and checks the declared pose count; mismatch raises `MMD_FLAG_VPD_COUNT_MISMATCH`.

## Hard constraints

The codec library code uses:

- C89-compatible source.
- No `malloc`, `free`, `realloc`, or heap ownership.
- No C `float` / `double` types.
- No `stdio` inside the codec.
- Caller-owned byte buffers and caller-owned output structs.
- Signed Q16.16 fixed-point for all vector, weight, color, and scalar numeric data.

PMD/PMX/VMD files store IEEE754 single-precision numbers on disk. This codec reads those 32-bit values as raw bits and converts them to Q16.16 with integer bit operations.

## Static capacity knobs

Override these at compile time if your engine has different budgets:

```c
#define MMD_MAX_VERTICES              8192
#define MMD_MAX_INDICES               24576
#define MMD_MAX_MATERIALS             256
#define MMD_MAX_TEXTURES              512
#define MMD_MAX_BONES                 512
#define MMD_MAX_IK_LINKS              2048
#define MMD_MAX_MORPHS                256
#define MMD_MAX_MORPH_OFFSETS         8192
#define MMD_MAX_RIGID_BODIES          512
#define MMD_MAX_JOINTS                512
#define MMD_MAX_SOFT_BODIES           128
#define MMD_MAX_SOFT_BODY_ANCHORS     1024
#define MMD_MAX_SOFT_BODY_PINS        4096
#define MMD_MAX_VMD_BONE_KEYS         4096
#define MMD_MAX_VMD_MORPH_KEYS        4096
#define MMD_MAX_VMD_CAMERA_KEYS       1024
#define MMD_MAX_VMD_LIGHT_KEYS        1024
#define MMD_MAX_VPD_BONES             512
```

When an asset exceeds a static limit, the loader keeps parsing the stream to preserve alignment, stores as much as fits, and raises an overflow flag.

## Build

```sh
make check CFLAGS='-std=c89 -O2 -Wall -Wextra -Werror -pedantic -Iinclude'
```

## Minimal usage

```c
#include "mmd_codec.h"

static MMD_Model model;
static MMD_Motion motion;
static MMD_Pose pose;

int import_asset(const unsigned char *bytes, mmd_u32 size)
{
    int fmt = mmd_probe(bytes, size);
    if (fmt == MMD_FMT_PMD || fmt == MMD_FMT_PMX) return mmd_load_model(bytes, size, &model);
    if (fmt == MMD_FMT_VMD) return mmd_load_vmd(bytes, size, &motion);
    if (fmt == MMD_FMT_VPD) return mmd_load_vpd(bytes, size, &pose);
    return MMD_ERR_MAGIC;
}
```

## Notes

- PMX material morph offsets are stored in the generic `MMD_MorphOffset` bank: `a` diffuse, `b.xyz` specular, `b.w` shininess, `c.xyz` ambient, `d` edge color, `e.x` edge size, `f` texture tint, `g` sphere tint, `h` toon tint.
- PMD materials do not have native material names; the codec stores texture and sphere references into the shared texture table and leaves material names empty.
- `.sph` maps to multiplicative sphere mode and `.spa` maps to additive sphere mode.
- Soft bodies are imported as data; Bullet simulation, skinning, and runtime physics solving are engine-side responsibilities.
- VPD names are treated as byte text. PMX UTF-16LE is transcoded to UTF-8; legacy Shift-JIS PMD/VPD names are preserved as bytes unless your engine adds its own Shift-JIS converter.
