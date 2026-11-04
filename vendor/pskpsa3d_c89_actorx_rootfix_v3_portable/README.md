
## ActorX root-index fix (2026-10-08)

**Important:** Do not classify the root using `parent_index >= 0`. The ActorX
`Bip01` root frequently has `parent_index == 0` and is therefore its own
parent sentinel. For this dataset, quaternion orientation conversion must
conjugate only bones whose **array index is nonzero**. This is applied both
to PSK bind bone rotations and PSA per-bone animation keys.

The core handles Q16.16-only skeletal transforms and skinning. The model is
Z-up. Do not rotate the mesh arbitrarily to "make it stand upright" in an
external renderer; instead configure the camera with Z as the up vector.

For a regression test run `make check` and check that
`test_actorx_root_vs_child: ok` appears. The new test uses non-identity root
and child quaternions, including root `parent_index = 0`.

### Inspect a full PSK/PSA skinning pose

```sh
make pskpsa_skin_dump
./pskpsa_skin_dump FemaleHobo.psk ZombieStandard.psa WalkF 12 0 points_q16.txt
```

The text file begins with the number of points, then one `x y z` Q16 integer
triple per line. Divide by 65536 in the external presenter. The CLI is
provided for **QA**; the library itself remains independent from stdio,
float/double, heaps and GPU APIs. Textures, UVs and faces are read separately
from the original PSK/MTL/PNG files in a renderer.

The `tools/pskpsa_skin_dump.c` utility uses stdio/stdlib but does not change
the codec's C89 fixed-point constraints.

# pskpsa3d

## v6 push

This build adds **PSA animation blend stack**, public pose blending/additive helpers, quaternion conjugate, and a final resolved **triangle stream** for renderer-facing output. It keeps v5 PSKX shape keys, PSA scale-key arrays, morph modules, and scale-aware skinning. The decoder core remains C89, heapless, and fixed-point only.

Codec 3D tiny para **ActorX PSK/PSA**:

- **C89**.
- **Sin `malloc`, `free`, `realloc`, heaps**.
- **Sin `float` ni `double`** en el core: los `float32` binarios del archivo se convierten a **Q16.16** con decodificador IEEE-754 entero.
- Parser desde memoria: el engine decide de dónde vienen los bytes.
- Estructuras estáticas con límites configurables por macro.
- Soporta PSK mesh/skeleton y PSA animsets con sampleo nearest-frame, NLERP y SLERP fixed-point.
- Incluye quaternion fixed-point, local-to-global skeleton pose, skinning final LBS de puntos y skin-cache para evitar barridos O(points*influences).

## Archivos

```text
include/pskpsa3d.h       API pública
src/pskpsa3d.c           implementación heapless fixed-point
tools/pskpsa_info.c      inspeccionador CLI opcional con stdio
tests/test_minimal.c     test autocontenido que genera PSK/PSA mínimos y prueba SLERP/cache/skinning/blending/stream
examples/embed_static.c  ejemplo de integración estática con cache
Makefile                 build simple

docs/SKINNING_FIXED.md       notas de skinning LBS/cache
docs/SLERP_FIXED.md          notas del SLERP entero
docs/PSKX_EXTRAS_AND_PLAYBACK.md
docs/MORPH_BLENDS_FIXED.md   shape keys, arrays y blend stack
docs/PSA_DEEPER_FIXED.md     scale keys PSA y track extraction
docs/ANIM_BLEND_AND_TRI_STREAM.md  PSA blend stack y salida final de triángulos
```

## Build

```sh
make
make check
```

En MSYS2/MinGW:

```sh
gcc -std=c89 -Wall -Wextra -pedantic -O2 -Iinclude -c src/pskpsa3d.c -o pskpsa3d.o
ar rcs libpskpsa3d.a pskpsa3d.o
gcc -std=c89 -Wall -Wextra -pedantic -O2 -Iinclude tools/pskpsa_info.c libpskpsa3d.a -o pskpsa_info.exe
```

## Uso rápido

```c
#include "pskpsa3d.h"

static psk3d_model g_model;
static psk3d_animset g_animset;
static psk3d_report g_report;
static psk3d_pose g_pose;

int load_pair(const unsigned char *psk, unsigned long psk_n,
              const unsigned char *psa, unsigned long psa_n)
{
    if (psk3d_parse_psk(psk, (psk3d_u32)psk_n, &g_model, &g_report) != PSK3D_OK) return 0;
    if (psk3d_parse_psa(psa, (psk3d_u32)psa_n, &g_animset, &g_report) != PSK3D_OK) return 0;
    if (psk3d_sample_sequence_nearest(&g_animset, 0u, 0u, &g_pose) != PSK3D_OK) return 0;
    return 1;
}
```

## Chunks soportados

### PSK

| Chunk | Contenido |
|---|---|
| `ACTRHEAD` | Header vacío del archivo PSK |
| `PNTS0000` | Puntos/posiciones, `FVector` -> Q16.16 |
| `VTXW0000` | Wedges: índice de punto + UV + material |
| `FACE0000` | Triángulos por índices de wedge |
| `MATT0000` | Materiales |
| `REFSKELT` | Huesos de referencia |
| `RAWWEIGHTS` | Pesos por punto/hueso |
| `EXTRAUVS[n]` | UV sets extra PSKX |
| `VERTEXCOLOR` | Color por wedge |
| `VTXNORMS` | Normales por wedge/punto |
| `MRPHINFO` | Metadata de shape keys / morph targets |
| `MRPHDATA` | Deltas de shape keys por punto |

### PSA

| Chunk | Contenido |
|---|---|
| `ANIMHEAD` | Header vacío del archivo PSA |
| `BONENAMES` | Huesos |
| `ANIMINFO` | Secuencias: nombre, grupo, frames, rate |
| `ANIMKEYS` | Keys: posición + quaternion + tiempo |
| `SCALE*` | Scale keys no estándar, opcionales |
| otros | se saltan sin romper el parseo |

## Animación y skinning fixed-point

Esta revisión ya no se queda en importer seco: incluye el pipeline final CPU-side para deformar la malla por huesos.

```c
psk3d_build_skin_cache(&model, &skin_cache);

/* Fast path: translation lerp + quaternion NLERP. */
psk3d_sample_sequence_linear(&animset, seq, frame, PSK3D_Q16_HALF, &local_pose);

/* Higher-quality path: translation lerp + quaternion SLERP. */
psk3d_sample_sequence_slerp(&animset, seq, frame, PSK3D_Q16_HALF, &local_pose);

psk3d_globalize_pose(model.bones, model.bone_count, &local_pose, &global_pose);
psk3d_skin_points_lbs_cached(&model, &skin_cache, &global_pose, &skinned_points);
```

Lo nuevo:

- `psk3d_q16_mul/div/sqrt/lerp`: math Q16.16 sin tipos flotantes.
- `psk3d_quat_normalize/mul/nlerp/slerp/rotate_vec3`: quaternion fixed-point.
- `psk3d_sample_sequence_linear`: interpola posición y quaternion con NLERP.
- `psk3d_sample_sequence_slerp`: interpola posición y quaternion con SLERP Q16.16.
- `psk3d_sample_sequence_linear_ex`: selector explícito `PSK3D_ROT_NLERP` / `PSK3D_ROT_SLERP`.
- `psk3d_globalize_pose`: transforma pose local a pose global por jerarquía de huesos.
- `psk3d_build_skin_cache`: precalcula bind global y listas de influencias por punto.
- `psk3d_skin_points_lbs_cached`: Linear Blend Skinning final usando `RAWWEIGHTS` sin recorrer todas las influencias por cada punto.
- `psk3d_skin_points_lbs`: wrapper cómodo que sigue funcionando sin preparar cache.

El resultado de skinning queda en `psk3d_skinned_points.points[]`; el renderer puede resolver cada triángulo con `faces -> wedges -> point_index`.

Ver también `docs/SKINNING_FIXED.md` y `docs/SLERP_FIXED.md`.


## V6: blend stack PSA + triangle stream final

V6 agrega una capa de runtime más cómoda encima del importer:

```c
static psk3d_anim_stack stack;
static psk3d_pose local_pose;

psk3d_clear_anim_stack(&stack);
psk3d_anim_stack_push(&stack, idle_seq, frame, t_q16,
                      PSK3D_Q16_ONE, PSK3D_ROT_NLERP,
                      PSK3D_BLEND_OVERRIDE);
psk3d_anim_stack_push(&stack, aim_seq, frame, t_q16,
                      PSK3D_Q16_HALF, PSK3D_ROT_SLERP,
                      PSK3D_BLEND_ADDITIVE);
psk3d_sample_anim_stack(&animset, &stack, &bind_pose, &local_pose);
```

También resuelve la malla skineada a un stream no-indexado, más fácil para debug renderers, software rasterizers o un backend OpenGL 1.x:

```c
static psk3d_triangle_stream stream;
psk3d_build_triangle_stream(&model, &skinned_points, &skinned_normals, &stream);
```

Cada `psk3d_final_vertex` trae posición, normal, UV principal, extra UV0, color, point/wedge/material original. Ver `docs/ANIM_BLEND_AND_TRI_STREAM.md`.

## Shape keys y blend modules

V5 agrega módulos separados para morphs PSKX:

```c
psk3d_morph_weights weights;
psk3d_skinned_points morphed;

psk3d_clear_morph_weights(&weights);
psk3d_set_morph_weight(&weights, &model, smile_index, PSK3D_Q16_HALF);
psk3d_apply_shape_keys_points(&model, &weights, &morphed);

/* O directo al skinning final: */
psk3d_skin_points_lbs_cached_morphed(&model, &cache, &global_pose, &weights, &skinned);
```

También hay un stack ligero de capas:

```c
psk3d_clear_morph_stack(&stack);
psk3d_morph_stack_push(&stack, blink_index, PSK3D_Q16_ONE);
psk3d_morph_stack_push(&stack, damage_index, PSK3D_Q16_HALF);
psk3d_apply_shape_key_stack_points(&model, &stack, &morphed);
```

Ver `docs/MORPH_BLENDS_FIXED.md`.

## PSA más neta

V5 también agrega:

- `scale_keys[]` opcionales desde chunks `SCALE*`.
- `psk3d_pose.scales[]` con identidad por defecto.
- Globalización de pose con escala jerárquica.
- Skinning de puntos con escala ósea.
- `psk3d_extract_sequence_track_range()` para leer rangos crudos por secuencia/hueso.
- `psk3d_sample_sequence_mapped_scaled_ex()` para ruta explícita de animación mapped + scale.

Ver `docs/PSA_DEEPER_FIXED.md`.

## Límites configurables

Antes de incluir `pskpsa3d.h`, define lo que necesites:

```c
#define PSK3D_MAX_POINTS     32768u
#define PSK3D_MAX_WEDGES     65536u
#define PSK3D_MAX_FACES      32768u
#define PSK3D_MAX_BONES      512u
#define PSK3D_MAX_KEYS          262144u
#define PSK3D_MAX_MORPH_DELTAS  131072u
#define PSK3D_MAX_MORPH_TARGETS 256u
#include "pskpsa3d.h"
```

O pásalo por compilador:

```sh
gcc -DPSK3D_MAX_KEYS=262144u ...
```

## Formato numérico

Todos los campos reales quedan como `psk3d_i32` **Q16.16**:

```text
1.0  -> 65536
0.5  -> 32768
-1.0 -> -65536
```

El core lee el patrón binario IEEE-754 de 32 bits y lo convierte con enteros. No declara variables `float` ni `double`.

## Notas de alcance

- Esto es un **decoder/importer + deformador CPU-side**, no un renderer.
- El sampleo de animación incluye **nearest-frame**, **linear fixed-point con NLERP** y **linear fixed-point con SLERP** de quaternions.
- El parser valida índices básicos: wedges -> points, faces -> wedges, influences -> points/bones, bone parents, y rangos de keys PSA.
- Soporta el layout ActorX clásico. Chunks extendidos/no estándar se registran como desconocidos y se saltan.

## Fuentes usadas para layout

- Unreal Wiki / BeyondUnreal: descripción general de PSK/PSA y chunks ActorX.
- UEViewer / ActorXImporter: structs de lectura para `VChunkHeader`, `VVertex`, `VTriangle`, `VMaterial`, `VBone`, `VRawBoneInfluence`, `AnimInfoBinary`, `VQuatAnimKey`.
- UEViewer / ExportPsk.cpp: orden y serialización de `ANIMINFO` y `ANIMKEYS`.
- Blender Extensions PSK/PSA: confirmación actual de uso/import-export de PSK/PSA en Blender.


## v5 API highlights

```c
psk3d_build_bone_map(&model, &animset, flags, &map);
psk3d_sample_sequence_mapped_ex(&model, &animset, &map, seq, frame, t, PSK3D_ROT_SLERP, &pose);
psk3d_cursor_reset(&cursor, seq);
psk3d_cursor_advance(&animset, &cursor, delta_seconds_q16, 1u);
psk3d_skin_normals_lbs_cached(&model, &cache, &global_pose, &skinned_normals);
psk3d_apply_shape_keys_points(&model, &weights, &morphed_points);
psk3d_skin_points_lbs_cached_from_points(&model, &cache, &global_pose, &morphed_points, &skinned_points);
psk3d_extract_sequence_track_range(&animset, seq, bone, &first_key, &key_count);
```

See `docs/PSKX_EXTRAS_AND_PLAYBACK.md` for details.

---

## v3: ABI portable y comprobaciones (2026-10-08)

**Protocolo estricto:** C89 sin C99 `<stdint.h>`, sin 64-bit como tipos del core,
sin heap y sin `float`/`double` en el core. La biblioteca usa `<limits.h>`
(ANSI C89) para exigir exactamente 8 bits por byte, 16 bits por `short` y
una pareja entera con signo/sin signo de exactamente 32 bits. En Win32 conserva
`long` de 32 bits de la ABI previa; en sistemas LP64 selecciona `int` de
32 bits, evitando que las estructuras y operaciones se expandan a 64 bits.
Si no existe ningún tipo válido, la compilación falla con `#error`.
`PSK3D_I32_STORAGE_LONG` indica cuál fue seleccionado (1 para `long`, 0 para
`int`). Las estructuras siguen usando el mismo API `psk3d_i32`/`psk3d_u32`.

La rotación raíz de ActorX V2 queda **sin cambios** (hueso 0 conserva el
cuaternión; los hijos conjugan XYZ). Se conserva el importador PSK/PSA, Q16.16,
skinning, animaciones, blending, morph targets y triangle streams.

Compilación y pruebas en GNU Make:

```sh
make clean
make CC=gcc CFLAGS='-std=c89 -Wall -Wextra -Werror -pedantic-errors -O2'
make check
```

En una shell MSYS2 MinGW32, se puede usar `./build_msys2_mingw32.sh`.
Alternativamente, `make EXEEXT=.exe` crea los ejecutables con sufijo Windows.
`make clean` elimina todas las utilidades y pruebas, tengan o no `.exe`.
Para verificar los límites numéricos existe `tests/test_numeric_limits.c`.

**Pendiente de QA externa:** confirmar animaciones y skinning visual con los
archivos reales PSK/PSA (p.ej. Female Hobo y Male Punk) y validar ejecutables
en una instalación real de MinGW32. La suite sintética no sustituye ese QA.
