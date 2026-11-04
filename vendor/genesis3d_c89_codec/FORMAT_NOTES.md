# FORMAT_NOTES

Estas notas documentan el modelo práctico usado por el codec.

## 1. `.ACT`

Contenedor VFS `VF00` con un `DirTree` estilo Genesis3D original. En disco hay una entrada raíz explícita con nombre vacío `""`; para la API se expone como `/`:

```text
/                 <- nodo raíz vacío en disco
├─ Header
├─ Body          (opcional)
└─ Motions/
   ├─ 0
   ├─ 1
   └─ ...
```

El parser también acepta los ACT legacy del kit donde `Header` estaba escrito directamente como primer nodo, sin raíz vacía.


### Lookup de VFS

El `DirTree` original de Genesis3D usa comparación case-insensitive y rutas con `\`. La API pública del kit acepta ambos separadores:

```text
Motions/0
MOTIONS\\0
header
```

Para inspección y tooling, el kit también expone contadores de árbol:

```c
g3d_vfs_count_entries(&vfs, 1); /* incluye directorios */
g3d_vfs_count_files(&vfs);       /* solo archivos con payload */
```

El CLI usa esto en `vfsls` y puede sacar payloads exactos con `vfsget`, tanto en `VF00` como en `CF00` cuando se pasa máscara/contexto.

### VFS `CF00`

`VF00` es el contenedor normal. `CF00` usa el mismo header y el mismo `DirTree`, pero los payloads de archivos hijos van XOR-eados con una máscara de un byte. Esa máscara no vive en el archivo; Genesis3D la deriva de un contexto externo en `fsvfs.c`.

El codec conserva el parser normal estricto: `g3d_vfs_parse()` rechaza `CF00`. Para abrirlo hay que llamar la variante explícita:

```c
g3d_u8 mask = g3d_vfs_context_mask_from_string("Genesis3");
g3d_actor_parse_file_with_mask(&actor, bytes, size, &arena, mask);
```

El writer puede emitir `CF00` para `.act` y `.bdy` con `g3d_actor_write_file_with_mask()` o `g3d_body_write_file_with_mask()`. Si la máscara es cero, se emite `VF00`.

### `Header`
Payload binario:

```text
u32 actor_sig
u32 actor_ver
u32 has_body
s32 motion_count
```

## 2. `.BDY`

Contenedor VFS `VF00` con raíz vacía original:

```text
/                 <- nodo raíz vacío en disco
├─ Geometry
└─ Bitmaps/
   ├─ 0
   ├─ 1
   └─ ...
```

El parser también acepta los BDY legacy del kit donde `Geometry` era el primer nodo directo. Además, `g3d_body_parse_geometry()` puede abrir directamente el subarchivo crudo `Geometry/BODY` sin VFS externo; `g3dtool` lo detecta como `BDY-GEOMETRY`.

> Nota: `Geometry/BODY` solo trae flags/materiales y geometría. Los bytes reales de bitmap viven en `Bitmaps/N`, por eso `extract` también exporta `bitmap_###.bin`.

### `Bitmaps/N` y `GeBm`

Los payloads `Bitmaps/N` son el formato escrito por `geBitmap_WriteToFile()` cuando la textura del material se serializa dentro del `.bdy`.

Header práctico del `GeBm` original:

```text
u32 'GeBm' little-endian = 0x6D426547
u8  version              = major<<4 | minor, Genesis3D 1.x usa 4.0
u8  info_flags
u8  pixel_format
u8  max_seek             = MaximumMip<<4 | SeekMipCount

if info_flags & WH_ARE_LOG2:
  u8 packed_dims         = log2(width)<<4 | log2(height)
elif info_flags & WH_ARE_BYTE:
  u8 width
  u8 height
else:
  u16 width
  u16 height

if info_flags & HAS_COLOR_KEY:
  color_key bytes        = bytes_per_pixel(pixel_format)

if info_flags & HAS_PALETTE:
  palette header + palette bytes

repeated:
  u8 mip_flags           = mip_index | compression/filter flags
  mip bytes              = mip_width * mip_height * bytes_per_pixel(pixel_format)
until mip_index > MaximumMip

if info_flags & HAS_ALPHA:
  nested GeBm alpha bitmap
```

`g3d_bitmap_probe()` inspecciona mips `store`/KIB/Paeth y reporta tamaño almacenado, método y banderas. El CLI `bitmap` inspecciona un `bitmap_###.bin` extraído y `materials` lo cruza contra los materiales de `.act`/`.bdy`.

### Decode `GeBm` a RGBA32 / TGA

La ruta de decode acepta mips raw y mips que necesitan scratch de arena:

```c
g3d_bitmap_decode_mip_rgba32_arena(bitmap_bytes, bitmap_size, 0,
    G3D_BITMAP_DECODE_APPLY_COLOR_KEY | G3D_BITMAP_DECODE_APPLY_ALPHA,
    rgba, rgba_cap, &width, &height, &rgba_bytes, &arena);

g3d_body_decode_material_rgba32_arena(&body, material_index, 0,
    G3D_BITMAP_DECODE_APPLY_COLOR_KEY | G3D_BITMAP_DECODE_APPLY_ALPHA,
    rgba, rgba_cap, &width, &height, &rgba_bytes, &arena);
```

Formatos raw cubiertos:

```text
8bit gray
8bit palettized + palette raw
16bit 555/565 RGB/BGR
16bit 4444/1555 ARGB
24bit RGB/BGR/YUV-as-RGB
32bit RGBX/XRGB/BGRX/XBGR/RGBA/ARGB/BGRA/ABGR
16/24bit UV/UVL como false-color para inspección
```

El CLI exporta previews/importables TGA 32-bit:

```sh
./g3dtool bmp2tga bitmap_000.bin bitmap_000.tga
./g3dtool materialtga player.act out_textures
./g3dtool materialtga player.bdy out_textures
```

`g3dtool extract` también intenta generar `bitmap_###.tga` junto al `bitmap_###.bin`. Los mips KIB/Paeth y la ruta wavelet8 experimental usan scratch de arena para mantener el núcleo sin asignación dinámica.

### `Geometry`
Payload binario:

```text
u32 body_sig
u32 body_ver
vec3 bbox_min
vec3 bbox_max

s16 xskin_vertex_count
xskin_vertices[]

s16 normal_count
normals[]

s16 bone_count
bones[]
strblock bone_names

s16 material_count
materials[]
strblock material_names

u32 lod_count
lod_face_lists[]
```

## 3. `.MOT`

Bloque binario:

```text
u32 'MTNB'
u32 motion_ver
u32 packed_flags
char name[name_len]

if leaf:
  s32 path_count
  s32 name_checksum
  u32 flags
  [events]
  [name_array]
  paths[]
```

### `.MOT` ASCII original `MOTN`

Genesis3D original escribe muchos `.mot` standalone como texto ASCII mediante `geMotion_WriteToFile()`. El codec ahora autodetecta también este encabezado:

```text
MOTN
 0.F0
NameID <name>
MaintainNames <0|1>
PathCount <n>
NameChecksum <checksum>
Events <0|1>
[TKEV ASCII]
NameArray <0|1>
[SBLK ASCII]
PathArray <n>
[PATH ASCII F0/F1/F2]*
```

`NameID` existe en el archivo original pero no cuenta dentro del loop de seis ids de `motion.c`; el parser replica esa heurística. `MOTION_NODE_BRANCH` permanece no soportado porque el branch original estaba incompleto en lectura/escritura.

### `PATH` ASCII F0/F1/F2

Formato moderno F1/F2 leído:

```text
PATH
 0.F2
Rotation <flag> <path_rotation_interp>
[QK Keys]
Translation <flag> <path_translation_interp>
[VK Keys]
```

En F1 las líneas `Rotation` y `Translation` pueden traer solo el flag.

Formato legacy F0 leído:

```text
PATH 0.F0
Looped <0|1>
Rotation <0|1>
CHNL 0.F0
InterpolationType <n>
StartTime <t>
EndTime <t>
KeyList <n>
[QKFL/QKFS/QKFQ keys]
Translation <0|1>
CHNL 0.F0
[VKFL/VKFH keys]
```

`QKFL`/`QKFS`/`QKFQ` usan quaternions en orden `W X Y Z`; `VKFH` trae vector base más derivadas. El codec mantiene la clave base en la representación pública y usa su sampler fixed-point para interpolar.

### `QKFrame` ASCII

```text
Keys <count> <qk_interp> <compression> <looping>
[start_time delta_time]      ; si LINEARTIME
[axis_x axis_y axis_z]       ; si HINGE
keys...
```

- sin compresión: `time W X Y Z`
- con `LINEARTIME`: `W X Y Z`
- con `HINGE`: `time angle`
- con `HINGE | LINEARTIME`: `angle`

El writer original añade textos finales como `Start T,Delta T` o `Axis`; se aceptan porque el parser lee los números iniciales.

### `VKFrame` ASCII

```text
Keys <count> <vk_interp> <compression> <looping>
[start_time delta_time]      ; si LINEARTIME
keys...
```

- sin compresión: `time X Y Z`
- con `LINEARTIME`: `X Y Z`

### `SBLK` ASCII

```text
SBLK
 0.F0
Strings <n>
string0
string1
...
```

Se convierte a la representación interna binaria de offsets + strings NUL-terminated.

### `TKEV` ASCII

```text
TKEV
 0.F0
DataSize <bytes>
TimeKeys <n>
<time> <offset>
<event-string>
...
```

Los eventos se reconstruyen en el buffer `event_data` y su `TKArray` de pares `(time_bits, offset)`. La API runtime permite leer texto y consultar por rango fixed16 con semántica `[start,end)`.

## 4. `Path`

Header empaquetado con:

- presencia de canal de traslación
- presencia de canal de rotación
- interpolador de traslación
- interpolador de rotación
- versión binaria

Luego:

```text
[path header]
[VKFrame opcional]
[QKFrame opcional]
```

## 5. Sub-bloques Genesis3D corregidos

### `SBKB` / `geStrBlock`

El source original (`Actor/strblock.c`) escribe el header binario como:

```text
u32 'SBKB'
s32 count
u32 size
u8  data[size]   ; tabla de offsets s32[count] + strings NUL-terminated
```

El parser actual valida la tabla de offsets y acepta como fallback el orden legacy `size,count` solamente cuando el layout oficial no cuadra. El writer siempre emite `count,size`.

### `QKFrame` / quaternions

La API pública usa:

```c
typedef struct g3d_fquat { x_bits, y_bits, z_bits, w_bits; } g3d_fquat;
```

Pero en disco Genesis3D serializa `geQuaternion` en orden:

```text
W, X, Y, Z
```

El codec reordena automáticamente:

```text
file WXYZ <-> API XYZW
```

Con esto los `.mot` del original y los escritos por el kit quedan alineados.

## 6. Validación

Los validadores del kit revisan, entre otras cosas:

- rangos de índices de triángulos
- correspondencia entre conteos y `strblock`
- integridad de `TKEvents`: offsets dentro de `event_data`, orden temporal y strings terminadas en cero
- keys explícitas de `QKFrame`/`VKFrame` en orden temporal estrictamente creciente
- `LINEARTIME` con `delta_time` positivo y conteo de tiempos coherente
- coherencia entre `count` y `time_value_count`
- presencia de buffers requeridos por compresión
- jerarquía de bones topológica: el padre debe ser anterior al hijo
- que un actor contenga body y motions válidos

## 7. CLI

`g3dtool` está pensado como paso natural encima del codec:

```text
info      -> resumen corto
dump      -> inspección más detallada
verify    -> parse + validate
verifycf  -> parse + validate de VFS CF00 con máscara/contexto
vfsls     -> lista árbol VFS, conteo de entradas y payloads
vfsget    -> extrae un subarchivo VFS exacto por ruta
bones     -> lista jerarquía de bones, bbox, vertex count y attachment translation
materials -> lista materiales, RGB, payload de bitmap y probe GeBm
bitmap    -> inspecciona un payload GeBm directo
motions   -> lista motions, paths, interpoladores y checksums de nombres
checksum  -> imprime checksums de strblocks/path names
extract   -> ACT -> BDY + Geometry crudo + bitmaps + MOTs
pack      -> BDY/Geometry crudo + MOTs -> ACT
canon     -> reserialización canónica
canoncf   -> CF00 -> VF00 canónico
makecf    -> VF00/CF00 ACT o BDY/Geometry crudo -> CF00
motascii  -> MOT binario/ASCII -> MOTN ASCII original-style
events    -> lista TKEB/TKEV de MOT/ACT en rango [start,end)
pose      -> sampleo de motion/actor
blendpose -> mezcla local de dos motions
skin      -> sampleo de malla deformada a OBJ
skinblend -> blend + skinning a OBJ
```

## 8. Filosofía del kit

- core sin memoria dinámica
- buffers del caller o buffers estáticos en el CLI
- fidelidad de bits para valores IEEE-754
- base preparada para futuras capas:
  - sampler runtime fixed-point
  - exportador debug a JSON
  - convertidor a glTF
  - más preservación de datos auxiliares en canales ASCII viejos

## 9. Sampler runtime fixed-point

El kit ya incluye una capa de sampleo runtime sin tipos FP binarios en la API:

```text
path   -> get_time_extents_fix16
path   -> sample_channels_fix16
motion -> get_time_extents_fix16
motion -> sample_channels_fix16
motion -> sample_channels_named_fix16
quat+pos -> matrix 3x4 fixed16.16
```

### Formato de salida

- traslación: `g3d_fixvec3` en `fix16.16`
- rotación: `g3d_q15quat` en `Q15`
- matriz: `g3d_fixmat34` en `fix16.16`

### Semántica implementada

- clamp para paths no loop
- wrap para paths loop
- lineartime compression en `VKFrame` y `QKFrame` como `start_time + index * delta_time`
- interpolación lineal de traslación
- hermite fixed-point con tangentes sintéticas
- rotación por blend normalizado fixed-point
- `QKFrame` con `hinge compression`: eje + ángulo -> quaternion Q15

### Límites actuales

- `SQUAD` no reproduce la ruta exacta del runtime original; hoy se resuelve por blend normalizado fixed-point
- el decode de hinge usa trigonometría fixed-point aproximada, suficiente para runtime ligero pero no bit-exact con FPU original

## 10. Pose blending

El kit agrega mezcla local de dos motions sobre el esqueleto del `.bdy`:

```text
body + motion A + motion B
  -> sample local pose A
  -> sample local pose B
  -> blend per-bone
  -> local_pose[]
```

### API relevante

- `g3d_body_sample_local_pose_fix16`
- `g3d_body_blend_local_pose_fix16`
- `g3d_body_local_pose_to_matrices_fix16`

### Criterio de matching motion -> bone

1. match exacto por orden si coincide el layout
2. fallback por nombre de bone/path
3. fallback por índice

### Blend implementado

- traslación: `lerp fix16.16`
- rotación: `nlerp Q15`
- curva de blend opcional:
  - `linear`
  - `hermite` (`smoothstep` fixed-point)

## 11. Skin sampling

El kit también samplea la malla deformada del `.bdy` usando la arena del caller:

```text
local pose[]
  -> matrices globales por bone
  -> transform de vertices/normals
  -> skinned mesh
```

### API relevante

- `g3d_body_skin_vertices_fix16`
- `g3d_body_alloc_skinned_mesh`
- `g3d_body_sample_skinned_fix16`
- `g3d_body_sample_skinned_blend_fix16`
- `g3d_actor_sample_skinned_fix16`
- `g3d_actor_sample_skinned_blend_fix16`

### Modelo de skinning usado

- un bone por vértice
- un bone por normal
- UV y topología copiadas del `.bdy`
- export OBJ desde LOD0 en el CLI

## 12. Smoke test sintético

`mk_sample` emite un corpus pequeño con:

```text
sample_body.bdy
sample_walk.mot
sample_strafe.mot
sample.act
```

Ese corpus se usa para probar:

- parse + validate
- extract + pack + canon
- vfsls/vfsget normal y CF00
- inspección bones/motions/checksum
- pose sampling
- pose blending
- skinning a OBJ
- skinning blend a OBJ

## 13. GeBm comprimido, Paeth y sub-lib KIB

Genesis3D marca cada mip de un `GeBm` con un byte donde los 4 bits bajos son el índice del mip y los bits altos pueden activar transformación de payload. Esta build reconoce:

```text
bit 4 / 0x10: MIP_FLAG_COMPRESSED
bit 5 / 0x20: MIP_FLAG_PAETH_FILTERED
```

La ruta nueva no cambia la semántica ACT/BDY/MOT; solo hace más profunda la capa de bitmaps/materiales.

### Metadatos nuevos de bitmap

`g3d_bitmap_probe()` ahora reporta cuánto ocupa realmente cada mip en disco y qué codec se detectó:

```c
g3d_bitmap_mip_info.stored_size;
g3d_bitmap_mip_info.codec_method;
g3d_bitmap_info.palette_stored_size;
g3d_bitmap_info.palette_codec_method;
```

Flags nuevos:

```c
G3D_BITMAP_INFO_HAS_COMPRESSED_MIPS
G3D_BITMAP_INFO_HAS_PAETH_MIPS
G3D_BITMAP_INFO_HAS_COMPRESSED_PALETTE
G3D_BITMAP_INFO_WAVELET_FORMAT
```

### API de desempaquetado

Para importadores conviene usar primero:

```c
int g3d_bitmap_unpack_mip(const void *data, g3d_u32 size, g3d_u32 wanted_mip,
                          g3d_u8 *dst, g3d_u32 dst_size,
                          g3d_u32 *width_out, g3d_u32 *height_out,
                          g3d_u32 *raw_bytes_out,
                          g3d_u32 *codec_method_out);
```

Esto devuelve bytes raw del pixel format original, ya descomprimidos y ya sin filtro Paeth. Después se puede usar:

```c
g3d_bitmap_decode_mip_rgba32(...)
g3d_body_decode_material_rgba32(...)
```

para RGBA32/TGA.

### Sub-lib `g3d_kib`

`g3d_kib` es una sub-lib C89 mínima para payloads comprimidos de textura:

```text
store       copia exacta
packbits    RLE tipo PackBits
lzss12      LZSS clásico con offset 12-bit / length 4-bit
wavelet8    ruta experimental gris 8-bit
```

Además trae:

```c
g3d_kib_paeth_filter(...)
g3d_kib_paeth_unfilter(...)
```

para fixtures y lectura de mips Paeth.

### Contenedor KIB0

Como el formato `GeBm` original no trae un tamaño comprimido explícito por mip, esta lib define un contenedor pequeño para assets nuevos o canonicalizados:

```text
'KIB0'
version
method
reserved
raw_size
payload_size
payload
```

La lectura intenta primero `KIB0`. Si no existe, hace autodetección best-effort LZSS12/PackBits. Para producir assets robustos desde esta lib, usar `KIB0`.

### Paeth

Si el mip trae `MIP_FLAG_PAETH_FILTERED`, el orden de lectura es:

```text
payload comprimido/raw
  -> descompresión si aplica
  -> Paeth unfilter por bytes-per-pixel
  -> bytes raw finales
```

### Paletas comprimidas

El flag de paleta comprimida se soporta para paletas KIB. El decode RGBA usa la paleta ya desempaquetada igual que una paleta raw.

### Wavelet

En el source Genesis3D entregado, `GE_PIXELFORMAT_WAVELET` aparece bloqueado como unsupported. Esta build no inventa compatibilidad bit-exact con un codec ausente: agrega una ruta experimental propia:

```text
G3D_PIXELFORMAT_WAVELET + KIB wavelet8
```

Sirve para fixtures y pipelines controlados que quieran meter texturas wavelet-like 8-bit. No debe tratarse como decode oficial de wavelet Genesis3D.
