# Giffany v11 — Bitmap/Material HyperProbe + body analytics

Este empuje vuelve a comparar el kit contra el source original de Genesis3D y baja una capa más: materiales y payloads `Bitmaps/N`.

## Cambios principales

### 1. Probe `GeBm` original-style

Se agregó un lector de metadatos para el formato que Genesis3D escribe con `geBitmap_WriteToFile()`:

```c
int g3d_bitmap_probe(const void *data, g3d_u32 size, g3d_bitmap_info *info);
```

El probe entiende:

- tag `GeBm` (`0x6D426547` little-endian)
- versión empaquetada `major<<4 | minor`
- `info_flags`
- `pixel_format`
- `MaximumMip<<4 | SeekMipCount`
- dimensiones empaquetadas:
  - `log2(width)<<4 | log2(height)`
  - `u8 width/u8 height`
  - `u16 width/u16 height`
- mips sin comprimir con sentinel `mip_index > MaximumMip`
- paleta simple no comprimida
- alpha como `GeBm` anidado

El probe marca como unsupported los casos donde no conviene fingir soporte:

- mips comprimidos/LZ
- filtros Paeth
- pixel format wavelet

### 2. Pixel formats públicos

Se añadieron constantes y helpers:

```c
G3D_PIXELFORMAT_8BIT_GRAY
G3D_PIXELFORMAT_16BIT_565_RGB
G3D_PIXELFORMAT_32BIT_ARGB
...

const char *g3d_pixel_format_name(g3d_u32 format);
g3d_u32 g3d_pixel_format_bytes_per_pixel(g3d_u32 format);
```

La tabla está alineada con `Bitmap/pixelformat.h` y `Bitmap/pixelformat.c` del Genesis3D original.

### 3. Helpers de materiales

Nuevas APIs:

```c
int g3d_body_find_material_index(const g3d_body *body, const char *material_name, g3d_s32 *out_index);
int g3d_body_find_material_index_prefix(const g3d_body *body, const char *material_prefix, g3d_s32 *out_index);
const char *g3d_body_get_material_name(const g3d_body *body, g3d_s32 material_index);
g3d_u32 g3d_body_count_textured_materials(const g3d_body *body);
int g3d_body_get_material_bitmap_info(const g3d_body *body, g3d_s32 material_index, g3d_bitmap_info *info);
int g3d_body_validate_bitmaps(const g3d_body *body, g3d_s32 *bad_material_index);
```

`g3d_body_find_material_index_prefix()` replica la semántica útil del original `geBody_GetMaterialByName()`, donde la búsqueda puede matchear por prefijo.

### 4. Helpers de body/bones

Nuevas APIs para importadores y depuración:

```c
int g3d_body_count_vertices_for_bone(const g3d_body *body, g3d_s32 bone_index, g3d_u32 *out_count);
int g3d_body_compute_bone_bbox_fix16(const g3d_body *body, g3d_s32 bone_index, g3d_fixvec3 *min_out, g3d_fixvec3 *max_out);
g3d_u32 g3d_body_lod_face_count(const g3d_body *body, g3d_u32 lod_index);
```

El comando `bones` ahora muestra también `verts=` y `computed_bbox=` por bone.

### 5. CLI nuevo

`g3dtool materials` inspecciona materiales desde `.act`, `.bdy` o `Geometry/BODY`:

```sh
./g3dtool materials player.act
./g3dtool materials player.bdy
./g3dtool materials geometry.body
```

`Geometry/BODY` crudo no incluye los payloads `Bitmaps/N`, así que ahí el CLI reporta `bitmap=missing-payload` en materiales texturizados.

`g3dtool bitmap` inspecciona un `GeBm` directo, por ejemplo un `bitmap_000.bin` exportado por `extract`:

```sh
./g3dtool bitmap out_actor/bitmap_000.bin
```

### 6. Samples y smoke más reales

`mk_sample` y `selftest` dejaron de usar bytes fake `BM 01 02` como bitmap. Ahora generan un `GeBm` mínimo válido:

```text
GeBm 4.0
2x2
8bit gray
1 mip sin comprimir
sentinel 0x0F
```

El smoke ahora valida:

- materiales de `.act`
- materiales de `.bdy`
- `bitmap_000.bin` extraído
- `g3dtool bitmap`
- `Geometry/BODY` con bitmap missing-payload
- bbox calculada por bone

## Verificación

Pasó:

```sh
make clean
make all
make test
make smoke
```

Resultado clave:

```text
OK actor_bytes=1129 body_bytes=534 motion_bytes=150/152 motions=2 vertices=3
smoke OK
```
