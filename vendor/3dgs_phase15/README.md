# 3D GameStudio A4–A8 — C89 Fase 13 — VTK directo

**Nuevo:** capturas VTK desde la geometría y skins del `.mdl` original, sin conversiones a OBJ/GLB. Véase `docs/PHASE13_VTK_DIRECT.md`. El puente C89 está en `src/bridge`, la captura en `tools/vtk_capture.py` y los tests en `scripts/check_phase13.sh`.

# 3D GameStudio A4–A8 · Fase 11 (C89 modular, source only)

Fase 11 prolonga el ZIP de la Fase 10 con **capas aditivas de referencia**, tanto
para morphs de MDL3/4/5 como para esqueletos MDL7, y **sincronización de
clips LOOP por fase normalizada**. Los cinco vendors conservan adaptadores
independientes; el núcleo reutilizable es compartido.

**Archivo nuevo:** `CHANGELOG_PHASE11.md`, `docs/PHASE11_ADDITIVE_SYNC.md`,
`SOURCES_PHASE11_DELTA.txt`, `scripts/check_phase11.sh`.

Compilar / probar: `sh scripts/check_phase11.sh` (Linux/macOS GCC/Clang).
Compilar con MinGW32: `sh BUILD_MINGW32_MSYS2.sh` (no probado en Win32 real).

Exportación rápida:

```
./gs3d_additive tests/fixtures/mdl7_skeletal_sequence.mdl pose 7 0 0 1 0 32768 1
./gs3d_additive tests/fixtures/mdl3_morph_fraction.mdl morph 4 0 0 1 0 32768 0
```

Las pruebas de geometría comercial animada todavía no certifican el sistema:
la regresión esquelética usa fixtures propios. No se ha modificado el Model
Runtime principal. El proyecto está limitado a una instancia global.

---

# 3D GameStudio A4–A8 — Fase 07 / C89 source only

Refactor incremental del proyecto de Fase 04. Cada vendor A4–A8 mantiene sus implementaciones
independientes de deformers y weights. **No integrado aún en el Model Runtime principal.**

## Cambios de esta fase

- `src/base/gs3d_file_sections.c`: valida el rango del MDL7 principal, el bloque ENT y el bloque MED.
  Los grupos no pueden leer bytes del editor MED. Evita tratar un MED lump como deformer.
- `src/mdl7/gs3d_mdl7_material.c`: conserva `Diffuse.r/g/b` y opacidad de `Ambient.a`
  del material binario, en Q16.16, evitando conversiones de float en C.
- `src/material/gs3d_material_query.c`: API agnóstica `gs3d_get_material()`;
  expone si hay imagen real y si existe material declarado.
- `src/export/gs3d_export_obj.c`: valores Kd y d auténticos cuando se encuentren; no
  exporta TGA sin dimensiones. Los modelos material-only generan OBJ y MTL válidos.
- `src/tools/gs3d_audit_main.c`: CLI `gs3d_audit` con formato, tamaño del archivo,
  grupos, UV, frames, deformers/pesos, secciones MDL7 y materiales.
- `tests/test_material_lumps.c`: casos de material sin imagen, MED válido, MED solapado y
  compatibilidad con skin legacy; `tests/make_fixtures.py` genera tres archivos sintéticos.

## Prueba con archivo REAL

Se obtuvo `Sphere_DiffPinkBlueSpec_Alpha90.mdl`, de 3,720 bytes, del repositorio público
`assimp/assimp` (test/models/MDL/MDL7 (3DGS A7)). Se comprobó con el binario de nuestro
parser compilado localmente, sin Assimp enlazado:

- MDL7, 1 grupo, 0 huesos, 62 vértices, 120 triángulos, 0 UV, 0 frames.
- Skin sin imagen raster, material explícito: Kd=0.9255 0.0000 1.0000;
  opacidad d=0.9000 (conversión Q16.16).
- OBJ/MTL generados y **sin TGA vacía**, corrigiendo el fallo de Fase 04.
- Hash SHA-256 del archivo real: `9755c55e41fd9b41ddbada658a4c11ff26112c1d4103528d162d03586b1e60ab`.

**El archivo de terceros NO está redistribuido en el ZIP**. Se puede obtener del proyecto de
origen, de acuerdo con sus condiciones de uso. Los fixtures incluidos sí son originales.

Esta muestra real **no posee deformadores ni huesos**. Por tanto, NO certifica pesos de MDL7,
ni confirma la colocación exacta del bloque de deformadores en subversiones reales.
El modelo de pesos sigue siendo experimental y solo se valida con nuestros fixtures sintéticos.

## Compilación y pruebas

Linux: `sh BUILD_LINUX_TEST.sh`

MSYS2 MinGW32: `sh BUILD_MINGW32_MSYS2.sh` o `BUILD_WIN32_MSYS2.bat`.
**Compilación Win32 no probada aquí; no hay compilador MinGW32 disponible**.

Herramientas:

```
./gs3d_audit character.mdl
./gs3d_export character.mdl outprefix
./gs3d_export character.mdl outprefix 0
```

- GCC + Clang, C89 estricto, `-pedantic -Wall -Wextra -Werror`.
- 6,231 truncamientos/mutaciones deterministas, y regresiones anteriores de pose,
  deformers, weights y adapters por generación.
- Clang AddressSanitizer+UBSan para pesos, materiales/lumps y robustez: pasaron.
- ISO C89, sin `malloc/calloc/realloc/free`, sin `float/double` en la biblioteca.
  El lector conserva buffers estáticos y NO es reentrante ni thread-safe.
- Archivos de modelos comerciales con pesos y MDL4 bones, deformer layouts alternos,
  materiales PBR/shaders, DDS y animación avanzada **no verificados**.

## Fuentes de referencia, no se copia su implementación

- https://github.com/assimp/assimp/tree/master/test/models/MDL
- https://github.com/assimp/assimp/blob/master/code/AssetLib/MDL/MDLFileData.h
- https://opserver.de/ubb7/ubbthreads.php?Number=467876&ubb=showflat
- https://www.manualshelf.com/manual/conitec/3d-gamestudio-a4/user-guide-english.html

## Novedades Fase 06

Consultar `CHANGELOG_PHASE06.md` para limites y verificaciones.

Nuevo muestreador de animacion morph (sin interpolacion o skinning de huesos):

```c
/* A7, grupo 0, fotogramas 0->1, mitad de tiempo (Q16.16). */
int r = gs3d_vendor_sample_morph(7U, 0U, 0U, 1U, 32768L);
```

`r == GS3D_OK` indica que la malla contiene el fotograma interpolado.
Para animacion MDL7 con claves esqueleto o deformadores de pesos,
la API morph-only responde `GS3D_ERR_UNSUPPORTED` en lugar de falsear el
resultado. Se recomienda un modulo futuro de interpolacion TRS esqueleto.

El fixture `mdl7_eight_groups_26.mdl` es sintetico pero reproduce la
organizacion cuantitativa de los 8 grupos de `PhosphoricAcid_MDl7.mdl`.
`docs/PUBLIC_CORPUS_HEADER_PHASE06.md` recoge los datos observados.
No se incorpora ningun archivo MDL comercial real en el paquete.

Pruebas adicionales: `tests/test_phase06.c` y `tests/test_phase06_robust.c`.
Los scripts de Linux y MinGW32 ya compilan los nuevos tests. El uso real de
MinGW32 no se ha probado en este entorno.


## Nuevas funciones Fase 07

La Fase 07 permite interpolación esquelética MDL7 en Q16.16. Consultar
`CHANGELOG_PHASE07.md` para una especificación técnica y limitaciones.

```c
/* Obtener la pose a mitad de un intervalo MDL7, en un grupo. */
int r = gs3d_sample_skeletal_q16(0U, 0U, 1U, 32768L);
/* Seleccionar vendor A7 explícitamente. */
r = gs3d_vendor_sample_skeletal(7U, 0U, 0U, 1U, 32768L);
```

```sh
sh BUILD_LINUX_TEST.sh
./gs3d_export tests/fixtures/mdl7_skeletal_sequence.mdl pose_50 skeletal 0 1 32768
```

Los fixtures son **sintéticos, originales**, por lo que la compatibilidad con
animaciones reales del editor MED sigue pendiente. Sin memoria dinámica,
`float`, `double`, ni `long long` en los lectores. El proyecto no está integrado
en el Model Runtime central.


## Fase 08 — Controlador de clips y secuencias

El paquete contiene `CHANGELOG_PHASE08.md` y `docs/CLIP_TIMELINE_PHASE08.md`.
No se infieren duraciones ni FPS nativos: el usuario proporciona `ticks_per_frame`.
Se soportan clamp, loop y ping-pong; selección por rangos, nombres y prefijos contiguos,
y salida por fotograma a OBJ/MTL con la CLI `gs3d_clip`.
Los cinco vendors disponen de adaptadores de clip independientes; las pistas con
huesos usan el motor de poses TRS de la Fase 07, y las demás pistas usan morph.


## Fase 09: crossfade independiente por vendor

La Fase 09 agrega 11 módulos C89 sin heap ni `float`, `double` o `long long`. Dos clips del mismo grupo y familia de pista se muestrean con relojes independientes. Los clips pueden tener duraciones distintas; el usuario indica explícitamente `ticks_per_frame` y `fade_ticks`. La transición se interpola en Q16.16.

### Compilar y probar solo los cambios de Phase09

```sh
sh scripts/check_phase09.sh
```

### Exportar 5 poses de una transición entre dos fotogramas esqueléticos

```sh
./gs3d_blend tests/fixtures/mdl7_skeletal_sequence.mdl /tmp/crossfade 7 0 0 1 1 1 100 100 5 2
```

Argumentos: `model prefix generation group from_first from_count to_first to_count ticks_per_frame fade_ticks samples track [all]`. `track=0` autodetección, `1` morph, `2` esquelético. Los clips de la herramienta se reproducen en bucle. `all` aplica índices de fotogramas compartidos a los grupos que los tengan, *previa comprobación de rangos*. Archivos de salida: `prefix_0000.obj`, ... con MTL y texturas cuando corresponda.

### API en C89

```c
GS3D_Clip idle, run;
GS3D_Crossfade fade;
GS3D_BlendSample pose;
/* Con el modelo ya abierto y los fotogramas definidos: */
gs3d_clip_create(&idle,7U,0U,0U,1U,100UL,
                 GS3D_CLIP_LOOP,GS3D_CLIP_TRACK_SKELETAL);
gs3d_clip_create(&run,7U,0U,1U,1U,100UL,
                 GS3D_CLIP_LOOP,GS3D_CLIP_TRACK_SKELETAL);
gs3d_crossfade_init(&fade,&idle,&run,0UL,0UL,100UL);
gs3d_crossfade_advance(&fade,50UL);
gs3d_vendor_crossfade_apply(7U,&fade,&pose);
/* pose.weight_q16 == 32768 */
```

### Diseño modular

- `src/blend/gs3d_crossfade_validate.c`: validación de clips y familias.
- `src/blend/gs3d_crossfade_player.c`: reloj y estado de transición.
- `src/blend/gs3d_crossfade_apply.c`: despacho individual y multigrupo.
- `src/blend/gs3d_blend_morph.c`: dos poses de vértices legacy.
- `src/blend/gs3d_blend_skeletal.c`: interpola TRS locales y deforma una vez.
- `src/vendors/a4/gs3d_a4_blend.c` ... `a8/gs3d_a8_blend.c`: adaptadores independientes.
- `src/vendors/gs3d_vendor_blend_router.c`: dispatcher genérico.

Los vendors A4/A5 no reciben artificialmente pesos de hueso; los vendors A6/A7/A8 pueden ejecutar la ruta MDL7 cuando el archivo realmente tiene datos esqueléticos. No se han certificado deformadores comerciales MDL7, ni se ha probado el compilador real Win32.


## Fase 10: Masked Animation Layers & Root Motion (C89)

- `docs/PHASE10_MASKED_LAYERS_ROOT_MOTION.md`: especificación, API, límites, ejemplos.
- `SOURCES_PHASE10_DELTA.txt`: once módulos nuevos de biblioteca, diseñados para expandirse.
- `src/layers/`: máscara de huesos, aplicación de capas, validación, skinning y deltas de raíz.
- `src/vendors/a4..a8/gs3d_aX_layer.c`: adaptadores individuales. A4/A5 rechazan correctamente el uso de esqueletos en MDL3–MDL5.
- `src/tools/gs3d_layer_main.c`: CLI para exportar una pose parcialmente mezclada.
- `scripts/check_phase10.sh`: pruebas específicas de la Fase 10 y regresión de la Fase 09.

El sistema usa desplazamiento de raíz CLAMP; los bucles todavía no se desenvuelven, la mezcla aditiva no está implementada y no se certifica una escena comercial MDL7 animada. Se conserva todo el código de fases anteriores.


## Fase 14: catálogo completo de skins

Consulta `docs/PHASE14_TEXTURE_CATALOG.md`. Ejecuta `sh scripts/check_phase14.sh` y `python3 tools/vtk_skin_catalog.py /carpeta/models /salida/vtk`. No requiere conversiones a OBJ.
