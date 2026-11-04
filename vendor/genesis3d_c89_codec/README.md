# Genesis3D / Reality Factory C89 codec kit

Kit de parser/códec, tooling y runtime fixed-point para los formatos binarios más importantes del pipeline de actores de Genesis3D / Reality Factory:

- `.act`
- `.bdy`
- `.mot`



## Parche Giffany v13: KIB Decomp + Paeth/Wavelet Push

Este empuje mete una sub-lib de descompresión para la ruta de texturas `GeBm`, siguiendo las banderas que usa `Bitmap/bitmap.c` de Genesis3D para mips comprimidos y filtrados. El objetivo es que el codec ya no solo inspeccione bitmaps: ahora puede **desempaquetar mips**, revertir **Paeth**, manejar **paletas comprimidas** y tener una ruta experimental para **wavelet 8-bit** sin contaminar el núcleo ACT/BDY/MOT.

Cambios principales:

- **Sub-lib `g3d_kib`**: `g3d_kib.h/.c` añade un contenedor C89 `KIB0` con métodos `store`, `packbits`, `lzss12` y `wavelet8`, más autodetección best-effort de payloads sin contenedor.
- **Descompresión de mip `GeBm`**: `g3d_bitmap_probe()` ya calcula `stored_size` y `codec_method` para mips con flag `compressed`; `g3d_bitmap_unpack_mip()` entrega bytes raw listos para decode/importador.
- **Paeth real**: mips con flag `MIP_FLAG_PAETH_FILTERED` pasan por `g3d_kib_paeth_unfilter()` después de copiar/descomprimir, usando stride por bytes-per-pixel.
- **Paleta comprimida**: paletas `GeBm` con flag de compresión se desempacan con KIB y luego alimentan el decoder RGBA.
- **Wavelet experimental**: Genesis3D 1.0 deja wavelet como unsupported; aquí queda una ruta `G3D_PIXELFORMAT_WAVELET + KIB wavelet8` para extracción gris 8-bit en pipelines propios.
- **CLI nuevo `bmpraw`**: exporta el mip desempaquetado a `.raw`, útil para depurar compresión antes de convertir a TGA.
- **Regresiones nuevas**: PackBits/KIB mip comprimido, Paeth filter/unfilter, paleta comprimida, wavelet8 experimental y `bmpraw` en smoke.

Detalles completos: [`CHANGELOG_GIFFANY_V13.md`](CHANGELOG_GIFFANY_V13.md).

## Parche Giffany v12: TexturePush RGBA/TGA decoder

Este empuje convierte el probe `GeBm` en una ruta práctica de extracción de texturas: ahora el codec puede decodificar mips sin comprimir a **RGBA32** y el CLI puede exportarlos como **TGA 32-bit** listo para revisar/importar.

Cambios principales:

- **Decoder `GeBm -> RGBA32`**: `g3d_bitmap_decode_mip_rgba32()` convierte mips raw de Genesis3D a RGBA, con soporte para gris, paleta, 555/565, 4444/1555, 24/32-bit RGB/BGR/ARGB/BGRA y formatos UV/UVL como false-color útil.
- **Color-key y alpha anidado**: flags `G3D_BITMAP_DECODE_APPLY_COLOR_KEY` y `G3D_BITMAP_DECODE_APPLY_ALPHA` aplican transparencia de color-key y del bitmap alpha embebido cuando el asset lo trae.
- **Decoder directo desde materiales**: `g3d_body_decode_material_rgba32()` evita rascar `Bitmaps/N` a mano.
- **CLI nuevo `bmp2tga` y `materialtga`**: exporta un `bitmap_###.bin` o todas las texturas decodificables de un `.act`/`.bdy` a TGA.
- **`extract` más útil**: además de `bitmap_###.bin`, ahora intenta dejar `bitmap_###.tga` junto al manifest.
- **Regresiones nuevas**: decoding RGBA de bitmap gris, decoding palettized `GeBm`, mip missing, `bmp2tga`, `materialtga` y TGA generado desde `extract`.

Detalles completos: [`CHANGELOG_GIFFANY_V12.md`](CHANGELOG_GIFFANY_V12.md).

## Parche Giffany v11: Bitmap/Material HyperProbe + body analytics

Este empuje baja otra capa hacia los assets reales de Genesis3D: ahora el kit no solo conserva los payloads `Bitmaps/N`, también puede reconocer el contenedor `GeBm` original usado por `geBitmap_WriteToFile()` y exponer metadatos útiles para tooling/importadores.

Cambios principales:

- **Probe `GeBm` original-style**: `g3d_bitmap_probe()` lee el header empaquetado real de Genesis3D (`flags`, `format`, `MaximumMip/SeekMipCount`, dimensiones log2/byte/u16), mips uncompressed, paleta simple y alpha como bitmap anidado.
- **Tabla pública de pixel formats**: constantes `G3D_PIXELFORMAT_*`, `g3d_pixel_format_name()` y `g3d_pixel_format_bytes_per_pixel()` para mapear materiales a formatos de textura.
- **Helpers de materiales**: `g3d_body_find_material_index()`, lookup por prefijo estilo `geBody_GetMaterialByName()`, `g3d_body_get_material_name()`, `g3d_body_count_textured_materials()`, `g3d_body_get_material_bitmap_info()` y `g3d_body_validate_bitmaps()`.
- **Analytics de body/bones**: conteo de vértices por bone, bbox calculada por bone y helper de conteo de caras por LOD.
- **CLI nuevo `materials` y `bitmap`**: inspecciona materiales/texturas dentro de `.act`/`.bdy`/`Geometry`, o un `bitmap_###.bin` extraído directamente.
- **Smoke actualizado**: ahora verifica `GeBm`, materiales, bitmap extraído, bbox calculada por bone y payloads missing en `Geometry/BODY` crudo.

Detalles completos: [`CHANGELOG_GIFFANY_V11.md`](CHANGELOG_GIFFANY_V11.md).

## Parche Giffany v10: HyperPush VFS + Lookups + Validación dura

Este empuje vuelve a apoyarse en el layout original de Genesis3D y convierte el kit en una herramienta más cómoda para destripar assets reales, no solo para parsearlos.

Cambios principales:

- **Inspector VFS público y CLI**: `g3d_vfs_count_entries()`, `g3d_vfs_count_files()`, `g3dtool vfsls` y `g3dtool vfsget` para listar/extractar subarchivos de `.act`/`.bdy`, incluyendo `CF00` con máscara/contexto.
- **Lookups runtime de body/motion/actor**: `g3d_body_find_bone_index()`, `g3d_body_get_bone_name()`, `g3d_body_compute_bbox_fix16()`, `g3d_motion_get_path_name()`, `g3d_actor_find_motion_index()` y `g3d_actor_get_motion_name()`.
- **CLI de inspección semántica**: `bones`, `motions` y `checksum` para revisar jerarquía de huesos, motions, paths, checksums de nombres y bounds sin escribir código auxiliar.
- **Validación más dura**: tiempos de keys explícitas deben ir estrictamente crecientes, `LINEARTIME` exige delta positivo, eventos `TKEV/TKEB` validan offsets ordenados y strings terminadas en cero, y bones exigen padres anteriores al hijo.
- **Smoke ampliado**: prueba `vfsls/vfsget`, extracción VFS normal y `CF00`, bones/motions/checksum, raw Geometry y comparación de payloads.

Detalles completos: [`CHANGELOG_GIFFANY_V10.md`](CHANGELOG_GIFFANY_V10.md).

## Parche Giffany v9: MaxPush Geometry + TKEV runtime

Este empuje sigue alineando el kit con el source original de Genesis3D: expone el subarchivo crudo `Geometry/BODY`, hace más útiles los eventos `TKEV/TKEB`, y endurece el parseo de motions contra bytes sobrantes.

Cambios principales:

- **Parser público de geometría cruda**: `g3d_body_parse_geometry()` lee directamente el payload `Geometry`/`BODY` que Genesis3D guarda dentro del `.bdy`.
- **Detección CLI `BDY-GEOMETRY`**: `g3dtool verify/info/dump/canon/pack/makecf` aceptan el subarchivo crudo `geometry.body`.
- **Extracción más completa de `.act`**: `g3dtool extract` ahora deja `body.bdy`, `geometry.body`, motions y bitmaps `bitmap_###.bin`.
- **Eventos de motion como API runtime**: `g3d_tkevents_get_text()`, `g3d_tkevents_count_between_fix16()`, `g3d_tkevents_find_next_between_fix16()` y wrappers `g3d_motion_*events*`.
- **CLI nuevo `events`** para listar eventos de `.mot` o `.act` dentro de `[start,end)`.
- **Parseo `.mot` más estricto**: bytes sobrantes al final de `MTNB/MOTN` ahora se rechazan como corrupción.
- **Regresiones nuevas** para raw Geometry, bitmaps `Bitmaps/N`, eventos `TKEV`, CF00 con bitmap, y pack desde geometry cruda.

Detalles completos: [`CHANGELOG_GIFFANY_V9.md`](CHANGELOG_GIFFANY_V9.md).

## Parche Giffany v8: Overdrive CF00 + ASCII writer

Este empuje suma escritura `.mot` ASCII original-style, lectura de `PATH 0.F0` legacy y compatibilidad práctica con VFS `CF00` en `.act`/`.bdy`.

Cambios principales:

- **Writer `.mot` ASCII `MOTN`** vía `g3d_motion_write_ascii()` y `g3dtool motascii`.
- **Parser `PATH` ASCII `0.F0` legacy** con `CHNL`, `QKFL/QKFS/QKFQ` y `VKFL/VKFH`.
- **VFS `CF00` con máscara Genesis3D**: APIs `*_with_mask`, cálculo de máscara desde contexto y soporte en CLI.
- **CLI nuevo para CF00**: `verifycf`, `canoncf`, `makecf`.
- **Regresiones nuevas** para round-trip ASCII, PATH viejo y contenedores CF00.

Detalles completos: [`CHANGELOG_GIFFANY_V8.md`](CHANGELOG_GIFFANY_V8.md).

## Parche Giffany v7: ASCII MOT codec push

Este empuje cierra una diferencia importante contra Genesis3D original: muchas tools originales escriben `.mot` sueltos como **ASCII `MOTN`**, aunque los motions dentro del `.act` suelen guardarse como **binario `MTNB`**.

Cambios principales:

- **Parser `.mot` ASCII `MOTN`** compatible con `geMotion_WriteToFile()`.
- **Parser `PATH` ASCII F1/F2** con bloques `Rotation`, `Translation` y `Keys`.
- **Parser `QKFrame` ASCII** con orden `W,X,Y,Z`, `LINEARTIME` y `HINGE_COMPRESSION`.
- **Parser `VKFrame` ASCII** con `LINEARTIME` como `start_time + delta_time`.
- **Parser `SBLK` ASCII** para name arrays escritos por el original.
- **Parser `TKEV` ASCII** para eventos de motion.
- **`g3dtool` puede verificar, samplear y canonicalizar `.mot` ASCII** a binario canónico.
- **`smoke.sh` ahora incluye un `.mot` ASCII original-style** y lo pasa por `verify`, `pose`, `canon` y `verify` otra vez.

Detalles completos: [`CHANGELOG_GIFFANY_V7.md`](CHANGELOG_GIFFANY_V7.md).

## Parche Giffany v6: Supersteroids codec push

Este paquete vuelve a empujar el codec contra el source original de Genesis3D y añade una capa extra de compatibilidad y runtime:

- **VFS/DirTree case-insensitive** como el `stricmp()` original; además `g3d_vfs_find()` acepta `/` y `\\`.
- **QKFrame hinge compression sampleable**: eje + ángulo se decodifican a quaternion Q15 para el sampler fixed-point.
- **LINEARTIME correcto** en `VKFrame` y `QKFrame`: Genesis3D guarda `start_time + delta_time`, no `start,end`.
- **NameChecksum Genesis3D**: nueva API `g3d_strblock_checksum()`; el writer de MOT auto-rellena el checksum si viene en cero.
- **Writers más duros**: motion/body/actor validan antes de serializar.
- **Build más estable**: `selftest`, `g3dtool` y `mk_sample` linkean contra `libg3d_codec.a` para no recompilar el núcleo varias veces.
- **Regresiones nuevas** para VFS con `\\`, búsquedas en mayúsculas/minúsculas, lineartime, hinge QK y checksum.

Detalles completos: [`CHANGELOG_GIFFANY_V6.md`](CHANGELOG_GIFFANY_V6.md).

## Parche Giffany v5: compatibilidad Genesis3D original “con esteroides”

Este paquete fue repasado contra el source original de Genesis3D incluido por Miguel. Los cambios fuertes están en `g3d_codec.c`, `selftest.c` y estas notas:

- **StrBlock `SBKB` oficial**: el parser ahora entiende el header original `Count, Size`; mantiene fallback para el orden legacy `Size, Count` que algunas builds previas de este kit escribían por error.
- **Quaternion `QKFrame` oficial**: Genesis3D serializa `geQuaternion` como `W, X, Y, Z`; la API pública sigue usando `g3d_fquat { x, y, z, w }`, así que el codec reordena al leer/escribir sin romper callers.
- **VFS/DirTree más fiel**: `.act` y `.bdy` ahora se escriben con la raíz vacía `""` tipo directorio, como el VFS original; el finder acepta tanto archivos con raíz original como archivos planos legacy del kit.
- **Regresiones nuevas**: `selftest` verifica `SBKB` oficial + legacy, orden `QKFrame` WXYZ y árbol VFS con raíz vacía.

Referencias locales usadas del source original:

```text
Genesis3D-master/Actor/actor.c
Genesis3D-master/Actor/body.c
Genesis3D-master/Actor/motion.c
Genesis3D-master/Actor/path.c
Genesis3D-master/Actor/QKFrame.c
Genesis3D-master/Actor/vkframe.c
Genesis3D-master/Actor/strblock.c
Genesis3D-master/Actor/tkarray.c
Genesis3D-master/Actor/tkevents.c
Genesis3D-master/VFile/fsvfs.c
Genesis3D-master/VFile/dirtree.c
Genesis3D-master/VFile/dirtree-common.c
```

> Nota: en los zips recibidos no apareció una carpeta `irrlicht-master`/`irrlitch-master`; el parche se aplicó al codec C89 `genesis3d_c89_codec`, que es el núcleo ACT/BDY/MOT encontrado.

## Restricciones del núcleo

- C89
- parsers ACT/BDY/MOT, KIB y textura con `arena` provista por el caller para todo buffer temporal
- sin tipos FP binarios en la API pública
- almacenamiento numérico como enteros, fixed point y/o bits IEEE-754 crudos
- parsing de assets principales sin depender de runtime externo pesado

## Qué trae

- `g3d_codec.h` API pública
- `g3d_codec.c` implementación del codec
- `g3d_kib.h` / `g3d_kib.c` sub-lib de descompresión para textura/bitmap
- `g3d_tool.c` CLI para inspección / extracción / empaquetado / pose / skinning
- `selftest.c` pruebas sintéticas
- `mk_sample.c` generador de assets de muestra
- `FORMAT_NOTES.md` notas de formato y límites prácticos
- `Makefile`

## Soporte implementado

### `.act`
- contenedor VFS `VF00` normal
- contenedor VFS `CF00` con máscara/contexto externo mediante APIs `*_with_mask`
- entrada `Header`
- entrada opcional `Body`
- directorio `Motions`
- motions numeradas `0..N-1`

### `.bdy`
- subarchivo crudo `Geometry` / `BODY` vía `g3d_body_parse_geometry()`
- contenedor VFS `VF00` normal
- contenedor VFS `CF00` con máscara/contexto externo mediante APIs `*_with_mask`
- entrada `Geometry`
- directorio `Bitmaps`
- probe y decode RGBA32 de `GeBm` sin comprimir
- geometría, bones, materiales, strblocks y LODs

### `.mot`
- motion binaria `MTNB`
- motion ASCII standalone `MOTN` del writer original Genesis3D
- nodo `leaf`
- name array binario `SBKB` y ASCII `SBLK`
- paths binarias y `PATH` ASCII F0/F1/F2
- `QKFrame` binario y ASCII, incluyendo WXYZ, lineartime y hinge
- `VKFrame` binario y ASCII, incluyendo lineartime
- `TKArray` binario
- `TKEB` binario y `TKEV` ASCII
- consulta runtime de eventos por rango `[start,end)`

## Mejoras de esta versión

### Runtime fixed-point
- `g3d_path_get_time_extents_fix16`
- `g3d_motion_get_time_extents_fix16`
- `g3d_path_sample_channels_fix16`
- `g3d_motion_sample_channels_fix16`
- `g3d_motion_sample_channels_named_fix16`
- `g3d_sample_xform_to_matrix`

### Pose blending y skin sampling
- `g3d_body_sample_local_pose_fix16`
- `g3d_body_blend_local_pose_fix16`
- `g3d_body_local_pose_to_matrices_fix16`
- `g3d_body_skin_vertices_fix16`
- `g3d_body_alloc_skinned_mesh`
- `g3d_body_sample_skinned_fix16`
- `g3d_body_sample_skinned_blend_fix16`
- `g3d_actor_sample_skinned_fix16`
- `g3d_actor_sample_skinned_blend_fix16`

### Validadores reutilizables
- `g3d_path_validate`
- `g3d_motion_validate`
- `g3d_body_validate`
- `g3d_actor_validate`

### Lookup / asset manager
- `g3d_motion_find_path_index`
- `g3d_motion_get_path_name`
- `g3d_motion_name_checksum`
- `g3d_body_find_bone_index`
- `g3d_body_get_bone_name`
- `g3d_body_bone_name_checksum`
- `g3d_body_compute_bbox_fix16`
- `g3d_body_find_material_index` / `g3d_body_find_material_index_prefix`
- `g3d_body_get_material_name`
- `g3d_body_count_textured_materials`
- `g3d_body_get_material_bitmap_info`
- `g3d_body_decode_material_rgba32`
- `g3d_body_validate_bitmaps`
- `g3d_body_count_vertices_for_bone`
- `g3d_body_compute_bone_bbox_fix16`
- `g3d_body_lod_face_count`
- `g3d_bitmap_probe`
- `g3d_bitmap_decode_mip_rgba32`
- `g3d_pixel_format_name` / `g3d_pixel_format_bytes_per_pixel`
- `g3d_actor_find_motion_index`
- `g3d_actor_get_motion_name`
- `g3d_vfs_count_entries` / `g3d_vfs_count_files`
- `g3d_tkevents_get_text`
- `g3d_motion_count_events_between_fix16`
- `g3d_motion_find_next_event_between_fix16`
- rechazo de duplicados
- `g3d_asset_manager_upsert`
- `g3d_asset_manager_count_type`

### CLI `g3dtool`
- `info`
- `dump`
- `verify`
- `vfsls`
- `vfsget`
- `bones`
- `materials`
- `bitmap`
- `bmp2tga`
- `materialtga`
- `motions`
- `checksum`
- `extract`
- `pack`
- `canon`
- `motascii`
- `events`
- `verifycf`
- `canoncf`
- `makecf`
- `pose`
- `blendpose`
- `skin`
- `skinblend`

## Build

```sh
make
make test
make smoke
```

## Uso rápido del CLI

### Ver resumen
```sh
./g3dtool info player.act
./g3dtool dump walk.mot
./g3dtool verify original_tool_output_ascii.mot
./g3dtool verify monster.bdy
```

### Inspeccionar VFS, huesos, motions y checksums
```sh
./g3dtool vfsls player.act
./g3dtool vfsget player.act Body body_from_actor.bdy
./g3dtool vfsget player.bdy Geometry geometry.body
./g3dtool vfsls player_cf.act Genesis3
./g3dtool vfsget player_cf.act Header header.bin Genesis3

./g3dtool bones player.act
./g3dtool bones geometry.body
./g3dtool materials player.act
./g3dtool materials geometry.body
./g3dtool bitmap out_actor/bitmap_000.bin
./g3dtool bmp2tga out_actor/bitmap_000.bin out_actor/bitmap_000.tga
./g3dtool materialtga player.act out_textures
./g3dtool motions player.act
./g3dtool checksum player.act
```

### Samplear una pose
```sh
./g3dtool pose walk.mot 0.5
./g3dtool pose player.act 0.5
./g3dtool pose player.act 1.25 0
```

### Blend de dos motions
```sh
./g3dtool blendpose player.bdy idle.mot 0.25 walk.mot 0.40 0.50
./g3dtool blendpose player.act 0 0.25 1 0.40 0.50 hermite
```

### Skinning a OBJ por frame
```sh
./g3dtool skin player.bdy walk.mot 0.5 out.obj
./g3dtool skin player.act 0.5 out.obj 0
```

### Skinning con blend a OBJ
```sh
./g3dtool skinblend player.bdy idle.mot 0.25 walk.mot 0.40 0.50 out.obj
./g3dtool skinblend player.act 0 0.25 1 0.40 0.50 out.obj hermite
```

### Extraer un `.act`
```sh
./g3dtool extract player.act out_actor
```

Salida típica:

```text
out_actor/
├─ body.bdy
├─ geometry.body
├─ bitmap_000.bin
├─ bitmap_000.tga
├─ 000_idle.mot
├─ 001_walk.mot
├─ 002_attack.mot
└─ manifest.txt
```

### Reempaquetar
```sh
./g3dtool pack rebuilt.act out_actor/body.bdy out_actor/000_idle.mot out_actor/001_walk.mot out_actor/002_attack.mot

# También acepta el subarchivo Geometry crudo; ojo: Geometry no trae payload de bitmaps.
./g3dtool pack rebuilt_from_geom.act out_actor/geometry.body out_actor/000_idle.mot
```

### Canonicalizar
```sh
./g3dtool canon weird_input.act clean_output.act
./g3dtool canon original_tool_output_ascii.mot clean_binary.mot
```

### Emitir MOT ASCII original-style
```sh
./g3dtool motascii clean_binary.mot original_style_ascii.mot
```

### Consultar eventos TKEV/TKEB
```sh
./g3dtool events walk.mot 0.0 1.0
./g3dtool events player.act 0.0 1.0 0
```

### Trabajar con VFS CF00
```sh
./g3dtool makecf Genesis3 player.act player_cf.act
./g3dtool verifycf Genesis3 player_cf.act
./g3dtool canoncf Genesis3 player_cf.act player_plain.act

# También acepta máscara explícita:
./g3dtool verifycf 0x67 player_cf.act
```

## Modelo de memoria

Todo parseo recibe una `g3d_arena` del usuario. El codec no reserva memoria dinámica por su cuenta.

```c
unsigned char arena_mem[1 << 20];
g3d_arena arena;
g3d_actor actor;

g3d_arena_init(&arena, arena_mem, sizeof(arena_mem));
g3d_actor_parse_file(&actor, bytes, byte_count, &arena);
```

### Buffers para skinning

El runtime de skinning escribe sobre buffers provistos por el caller o reservados dentro de la arena:

```c
g3d_skinned_mesh mesh;

if (g3d_body_alloc_skinned_mesh(&body, &arena, &mesh) == G3D_OK) {
    g3d_body_sample_skinned_fix16(&body, &motion, time_fx, &mesh);
}
```

## Fixed point

Se incluyen helpers para convertir entre bits IEEE-754 y `fix16.16` sin tipos FP binarios en la API pública:

```c
g3d_fix16 fx = g3d_f32bits_to_fix16(0x41200000u); /* 10.0f */
g3d_u32 bits = g3d_fix16_to_f32bits(fx);
```

## Asset manager mínimo

```c
g3d_asset_slot slots[32];
g3d_asset_manager mgr;

g3d_asset_manager_init(&mgr, slots, 32);
g3d_asset_manager_add(&mgr, &arena, "player.act", G3D_ASSET_ACTOR, &actor);
g3d_asset_manager_upsert(&mgr, &arena, "player.act", G3D_ASSET_ACTOR, &actor);
```

## Limitaciones intencionales

- `GeBm` probea metadatos y desempaqueta mips `store`/KIB/Paeth; la ruta wavelet8 sigue marcada como experimental
- `CF00` requiere que el caller entregue la máscara/contexto; Genesis3D no guarda esa máscara dentro del archivo
- `QKFQ`/`VKFH` legacy se aceptan para compatibilidad, pero sus datos auxiliares no se preservan como canal público independiente
- no soporta motions `branch`/compuestas como formato serializable del codec
- `SQUAD` se resuelve como blend normalizado fixed-point; no reproduce todavía la curva exacta del runtime original
- para rotación usa una ruta fixed-point tipo `nlerp` para `linear/slerp/squad`
- para traslación hermite usa tangentes estilo Catmull-Rom; `zero-deriv` fuerza tangentes cero
- el skinning implementado usa el modelo clásico visible en `BDY`: un `bone index` por vértice y normal
- el CLI usa buffers estáticos de tamaño fijo para evitar llamadas de asignación dinámica

## Notas prácticas

- `.act` se trata como contenedor final de actor.
- `.bdy` se trata como body serializado con VFS interno.
- `.mot` apunta al layout binario estable usado por runtime.
- `blendpose` deja inspeccionar mezcla local de huesos sin exportar malla.
- `skin` y `skinblend` escriben `.obj` con vértices, UVs, normales y caras LOD0.
- `extract` deja un `manifest.txt` para reconstruir rápido el `.act`.
- `materials` y `bitmap` ayudan a auditar texturas `GeBm` sin levantar Genesis3D.
