**Current extension: v0.25 — see [README_V025.md](README_V025.md).**

# RE3 Codec v0.23 — comprobación visual del morph ponderado Q12

Esta versión **no modifica la operación morph del núcleo v0.22**; incorpora un visor que **la usa realmente**, con pesos explícitos y un ensayo `pc-pulse` de aumento y disminución de peso. El modo anterior `native/recenter/delta` permanece disponible.

El último argumento optativo del visor es `pc:INDEX:WEIGHT` (índice de delta, peso Q12 de 0 a 4096) o `pc-pulse:INDEX` (peso de prueba, 0 → 4096 → 0 según el índice de key). Debe indicarse `rest` y no utilizar timeline externo para probar la mezcla PC. Ejemplos:

```sh
make -j4
mkdir -p frames
./re3emd_anim_preview EM30.EMD EM30.TIM frames 0 4 18 quarter texture model main rest no-map - native pc-pulse:0
./re3emd_anim_preview EM36.EMD EM36.TIM frames 0 4 1 quarter texture model main rest no-map - native pc:0:2048
./re3emd_weighted_morph_audit EM30.EMD
make test-weighted-corpus CORPUS_DIR=/ruta/corpus
```

La cámara del modo `pc:INDEX:WEIGHT` usa los límites combinados de cinco pesos del mismo canal, **para evitar que el auto-zoom oculte deformaciones**. `pc-pulse` mantiene el encuadre fijo dentro del clip, pero sus cambios de peso son ilustrativos, no la curva temporal verdadera de Capcom. `INDEX` es el índice de desplazamiento **dentro de cada pista de la parte**; si una pista no tiene ese canal, permanece en su estado 0.

La herramienta de auditoría prueba **cada vértice con morph**, cinco pesos por cada canal y mezclas de los dos primeros canales cuando existen; compara contra un cálculo entero independiente. Las comparaciones muestran diferencias en varias posiciones base de binary01 respecto al MD2 que la aproximación v0.21 omitía. El visor es una previsualización CPU, no un emulador GPU de PlayStation.

**Limitaciones:** los pesos y los estados visibles todavía necesitan trazas de runtime/SCD; la fidelidad exacta del rasterizador, las reglas de daño y el comportamiento final de cada parte no están certificados. No se agregaron recursos comerciales al código.

---

# RE3 Codec v0.21 — hipótesis de morphs aditivos (C89)

La v0.21 añade un **modo experimental y reversible** `delta` para estados morph posteriores al 0. Las pruebas visuales con EM30/EM33 muestran que sumar sus valores a la geometría base conserva la continuidad del Grave Digger mejor que interpretarlos como posiciones absolutas. También se comparó EM36. **El motor original aún no se ha descompilado para probar esta semántica.**

Las APIs previas **no cambian su valor por defecto**. Para probar el modo nuevo:

```sh
make -j2
mkdir -p frames
./re3emd_anim_preview EM30.EMD EM30.TIM frames 0 4 10 quarter texture model main morph:1 no-map - delta
./re3emd_morph_space_audit EM30.EMD em30_morphs.csv
./test_morph_delta EM30.EMD
```

Para compararlo con las variantes `native` y `recenter`, cambia el último parámetro. La opción `-` representa *sin archivo externo de eventos*. Al utilizar un timeline, `delta` también funciona por pieza.

**El modo `native` mantiene la interpretación absoluta de la v0.20; `recenter` traslada el centro de cada morph al del mesh original; `delta` suma los valores del morph a los vértices locales originales (en frame 0 conserva la interpretación absoluta).** Los tres son modos diagnósticos y no se deben mezclar con una afirmación de equivalencia al juego sin verificación adicional.

La auditoría completa del corpus se encuentra en `AUDITORIA_RE3_V021_MORPH_COORDINATE_HYPOTHESES.md`.

---

# RE3 Codec v0.19 — Estados explícitos de piezas y morphs

La v0.19 añade `re3c_visual_state`, control per-part de visibilidad/morph y un *timeline* externo opcional para `re3emd_anim_preview`. También añade `re3scd_parts_probe` para decodificar estructuralmente una instrucción `PARTS_SET` en un offset especificado de un SCD extraído. **No atribuye eventos del juego ni ejecuta SCD completo**. Consulta `AUDITORIA_RE3_V019_STATES_SCD.md`.

Ejemplo (carpeta `out` ya creada):

```sh
re3emd_anim_preview EM36.EMD EM36.TIM out 0 1 16 front texture model main rest no-map examples/EM36_explicit_state_demo.txt
```

# RE3 Codec v0.17 — Auditoría Némesis y zombis

La v0.17 añade `re3emd_fidelity_audit` (auditoría exhaustiva por key y muestreo de vértices y UV) para exponer los bancos incompatibles y los morphs no aplicados. **No se modificó la semántica de skinning respecto a v0.16.**

```sh
make -j2
./re3emd_fidelity_audit EM36.EMD EM36.TIM
```

# RE3_codec v0.16.0 — animated EMD/TIM previews, fixed camera, root motion diagnostics

Dedicated Resident Evil 3 (1999) EMD/TIM decoder/runtime support library.

**v0.16.0 changes:** Real EDD/EMR keyframes now drive the `re3emd_anim_preview` software renderer; v0.16 also adds the `re3c_animated_body_vertex` core API, clip-stable framing, `model` vs `frame` root motion modes, incompatible-rig protection and `re3emd_motion_audit`. Verified with GCC/Clang C89 and 100 EMD corpus smoke tests. See `AUDITORIA_RE3_V016_ANIMATION.md`.

**v0.15.0 changes:** `re3c_reference_body_vertex` reconstructs native reference-pose positions for DAT-owned vertices, Q0.7 seams, and rigid joint components. `re3emd_preview` produces textured, depth-tested BMP files, triangulates RE3 quads using 0-1-3-2, and defaults to hiding unbound/morph auxiliary geometry in previews without deleting it. `re3emd_reference_audit` reports reference-pose coverage. Tested on 100 EMD+TIM pairs; see `AUDITORIA_RE3_V015_RENDER_REFERENCE.md`.

**v0.14.0 change:** Treat absent/4-byte EDD animation-bank sections as optional, rather than reporting false static overflow. 100 EMD / 100 TIM / 100 EMD+TIM corpus test sets now pass. See `AUDITORIA_RE3_V013_OPTIONAL_EDD.md`.


## Recovery status

This tree is a **recovered/reconstructed v0.13.0 source tree** made after the
original chat attachment lost its downloadable backing file. It is not claimed
to be byte-identical to the lost ZIP. The recovered core/header reproduce the
v0.13 data model and format findings that survived in the session record; the
small command-line wrappers/tests were rebuilt around that API so the package
is usable again.

The lost original ZIP had SHA-256:

`10df9056697f0f45625b4c21dfaeee912a73ee1c0229b8f9f505b8e50400e8f8`

Keep that hash as an identity marker for the missing original. See
`recovery_evidence/RECOVERY_STATUS.md`.

## Design constraints

* ISO C89 (`-std=c89 -pedantic -Wall -Wextra -Werror`)
* fixed-point/integer runtime math only
* no `malloc`, `calloc`, `realloc`, `free`, or heap ownership
* no `float`, `double`, 64-bit integer types, or `long long`
* static bounded capacities
* `long` appears only at the C89 `fseek`/`ftell` boundary

## Supported RE3 material

* 15-entry RE3 EMD section directory
* EDD2-style clip/key timelines
* EMR hierarchy/rest pose and packed 12-bit rotations
* Normal A / NormalEx B / Damage / auxiliary animation banks
* RE3 MD2 body and auxiliary meshes
* Binary00 ownership + seam binding streams
* Binary01 vertex morph tracks
* raw bounded access to any EMD section (`re3c_read_section`)
* PSX TIM loading into a fixed 1024x512 VRAM image
* 4/8/16-bpp TIM sampling using TPAGE/CLUT
* PSX 15-bit color conversion preserving `0x8000` as visible black
* body-part diagnostic classification
* fixed Q0.7 seam blending

## Build

```sh
make
make test
```

Windows / MinGW:

```bat
build_mingw.bat
```

For a retail corpus:

```sh
make test-corpus CORPUS_DIR=/path/to/emd
make test-tim-corpus TIM_DIR=/path/to/tim
make test-texture-corpus CORPUS_DIR=/path/to/paired/files
```

## Tools

* `re3emd_info` — section, bank, mesh and structural summary
* `re3emd_parts` — body part ownership/classification report
* `re3emd_morph` — Binary01 morph-track report
* `re3emd_viewer` — legacy/raw-space wireframe BMP diagnostic (unposed)
* `re3emd_preview` — C89 depth-tested, optionally textured reference-pose BMP preview (`front`, `quarter`, `profile`, `back`)
* `re3emd_reference_audit` — inspect translated and DAT-bound vertices / optional parts; exit nonzero on errors
* `re3emd_anim_preview` — render animation frames (BMP) using real EDD/EMR poses; stable camera, choose root mode
* `re3emd_motion_audit` — sample three keys per compatible clip and check transforms/vertex output

## Important semantic boundary

RE3 SCD opcodes such as `PARTS_SET`, `PARTS_BOMB` and `PARTS_DOWN` are script
commands and are not treated as an invented bytecode inside Binary00/01. The
Binary00 vertex-bind stream is decoded using the ownership/seam grammar found
from retail data; Binary01 is treated as a morph container when its validated
header/layout is present.

## Visual check in Windows

Run `build_mingw.bat` in your 32-bit MSYS2 MinGW environment, then:

```bat
re3emd_preview.exe "C:\RE3\EM34.EMD" "C:\RE3\EM34.TIM" "C:\RE3\nemesis.bmp" front texture main
re3emd_preview.exe "C:\RE3\EM34.EMD" "C:\RE3\EM34.TIM" "C:\RE3\nemesis_all.bmp" front texture all
re3emd_reference_audit.exe "C:\RE3\EM34.EMD"
```

The default `main` excludes optional extra/unbound and morph geometry **only in previews**. Use `all` to inspect them. No original Capcom data files are bundled in this source ZIP.

This is an integer software diagnostic renderer (orthographic, affine UV, simplified depth and no GTE lighting), not a pixel-identical reproduction of the retail game. Static reference pose only: animation blending and dynamic morphs are future work.

## Animated previews (v0.16)

Use output directories that already exist. Arguments:

```
re3emd_anim_preview model.EMD texture.TIM OUTDIR BANK CLIP [MAX_FRAMES] [front|quarter|profile|back] [solid|texture] [model|frame] [main|all]
```

Normal animation bank is `0`. Clip IDs are zero-based, not animation names.

**Example in Windows (MinGW32):**

```bat
build_mingw.bat
mkdir nemesis_frames
re3emd_anim_preview.exe EM34.EMD EM34.TIM nemesis_frames 0 23 12 front texture frame main
re3emd_motion_audit.exe EM34.EMD
```

Choose `frame` to include frame-encoded root movement (`speed` and `origin_y`), or `model` to remove root translation while examining bone rotations. The actual original PC engine may process its root fields in additional ways; these are diagnostic modes, not proven Capcom-equivalent playback.

The B/damage/weapon bank may use a distinct hierarchy. The preview intentionally rejects joint-count mismatch rather than treating incompatible skin weights as if the original skeleton were identical. `EM34` has 16 Normal joints and 15 Damage joints, so damage clips are **not yet renderable correctly** with this helper. Morph/optional mesh pieces and SCD PARTS_* commands remain separate future work.

To generate a GIF, use an external image converter on the numbered BMP files; the runtime itself has no third-party media library dependency. The source distribution does not include game data.


## v0.20 — catálogos de animaciones de criaturas

Dos nuevas herramientas C89, de memoria estática y sin dependencias externas:

```text
re3emd_clip_catalog criatura.EMD salida.csv
re3emd_compare_clips EM23.EMD EM28.EMD
```

Revisan todas las keys y comparan poses reales. Para un informe completo y resultados, consulta `AUDITORIA_RE3_V020_CRIATURAS.md`. Los bancos de daño que requieren retarget NO se consideran automáticamente válidos para el cuerpo. Se corrigió también el script `build_mingw.bat`.
