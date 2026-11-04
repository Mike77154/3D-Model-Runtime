# Giffy RE Codec C89

Librería C89 autocontenida para importar/exportar modelos clásicos de **Resident Evil 1, 2 y 3** hacia un formato intermedio propio (`.gfm`) y formatos de trabajo (`.obj`, `.tmd`, `.md1`, `.md2`, `.emd`, `.pld`, `.plw`). Incluye CLI de conversión, limpieza/validación de malla y un visor puro por software que renderiza wireframe a `.ppm`.

> Estado: **v0.3 experimental, enfocada en modelos clásicos RE2/RE1/RE3**. No incluye assets de Capcom ni código copiado de repos externos. La exportación nativa es un subconjunto técnico para tooling/roundtrip; para reinserción retail real todavía hay que validar empaquetado, animaciones, armas, textura/CLUT y límites por versión.


## Novedades de v0.3

```text
+ Parser MD1/RE2 reescrito con layout BioModels: descriptor real de 56 bytes por limb.
+ EMD/PLD/PLW RE2 detectan malla MD1 en los offsets usados por BioModels.
+ PLD/PLW RE2 ya se etiquetan como RE2, no como fallback RE3/RE1.
+ Se conserva una pieza por limb, con triángulos y quads compartiendo vértices/normales.
+ Lectura básica de EMR/skeleton para parent_index y pivots por pieza cuando existe.
+ TIM interno de PLD/PLW y TIM sidecar de EMD se adjuntan al modelo como textura RGBA cuando están disponibles.
+ Materiales placeholder por CLUT id: re2_clut_N.
```

## Novedades de v0.2

```text
+ GFM2: formato intermedio con tabla de piezas/limbs.
+ OBJ import/export con grupos y MTL básico.
+ CLI validate / clean / sections.
+ Estadísticas: tris, quads, caras inválidas, degeneradas, vértices sin usar.
+ Limpieza de malla: drop invalid, drop degenerate, remove unused, weld.
+ Preparación PSX: triangulate, flat normals, quantize i16-style, flip V.
+ Parsers native RE mejorados para conservar piezas por objeto/limb cuando es posible.
+ Writer MD2/RE3 corregido para roundtrip sin degenerar el cubo de prueba.
+ Visor puro mejorado: color por cara, grid y proyección xy/xz/yz.
+ Muestras: cube.obj y two_piece.obj/.mtl.
```

## Estructura

```text
include/giffy_recodec.h       API pública C89
src/giffy_recodec.c           implementación single-library
tools/grc.c                   CLI de info/validate/convert/clean/sections/extract/tim2ppm
viewer/giffy_re_view.c        visor wireframe sin SDL/OpenGL
samples/cube.obj              modelo de prueba básico
samples/two_piece.obj         muestra con grupos/materiales
samples/two_piece.mtl         MTL de la muestra por piezas
docs/FORMAT_GFM.md            formato intermedio GFM1/GFM2
docs/INTEGRATION.md           guía para meterlo en engine/editor
docs/RE_NATIVE_NOTES.md       notas RE1/RE2/RE3 y límites
docs/API_REFERENCE.md         mapa rápido de API
docs/MODEL_UPGRADE_NOTES.md   notas de la mejora v0.2
Makefile                      build Unix-like
```

## Build

```sh
make
```

Genera:

```text
build/libgiffy_recodec.a
build/grc
build/giffy_re_view
```

## Smoke test

```sh
sh tests/smoke.sh
```

El test compila, importa/exporta OBJ/GFM/TMD/MD1/MD2/EMD/PLD, valida mallas, inspecciona secciones nativas y genera previews `.ppm`.

## CLI

```text
grc info <model> [--scale n]
grc validate <model> [--scale n]
grc convert <input> <output> [--game re1|re2|re3] [--scale n] [--clean] [--triangulate] [--normals] [--weld eps] [--psx] [--flipv]
grc clean <input> <output> [--triangulate] [--normals] [--weld eps] [--psx] [--flipv]
grc sections <emd/pld/plw>
grc extract <emd/pld/plw> <prefix>
grc tim2ppm <input.tim> <output.ppm> [palette]
```

### Ejemplos

```sh
./build/grc info samples/two_piece.obj
./build/grc convert samples/two_piece.obj build/two_piece.gfm --normals
./build/grc convert build/two_piece.gfm build/two_piece.obj
./build/grc convert samples/cube.obj build/cube.gfm --clean --normals
./build/grc convert build/cube.gfm build/cube.tmd --psx
./build/grc convert build/cube.gfm build/cube.md1 --psx
./build/grc convert build/cube.gfm build/cube.md2 --psx
./build/grc convert build/cube.gfm build/cube_re1.emd --game re1 --psx
./build/grc convert build/cube.gfm build/cube_re2.emd --game re2 --psx
./build/grc convert build/cube.gfm build/cube_re3.emd --game re3 --psx
./build/grc sections build/cube_re3.emd
./build/giffy_re_view build/cube.gfm build/cube_xy.ppm 512 512 xy
./build/giffy_re_view build/cube.gfm build/cube_xz.ppm 512 512 xz
```

## Soporte por formato

| Formato | Import | Export | Estado |
|---|---:|---:|---|
| `.gfm` | sí | sí | GFM2 conserva piezas/limbs, materiales básicos y texturas RGBA embebidas |
| `.obj` | sí | sí | grupos `o/g`, `usemtl`, MTL básico, triangulación fan |
| `.tmd` | sí | sí | subset PS1/TMD de malla flat; importador heurístico para primitivas |
| `.tim` | decode a RGBA/PPM | no | 4/8/16/24 bpp básico |
| `.emd` RE1 | sí | sí | experimental; exportador usa mesh compatible roundtrip |
| `.emd` RE2 | sí | sí | experimental; mesh section 7 estilo BioModels, TIM sidecar opcional |
| `.emd` RE3 | sí | sí | experimental; vertex binding/skinning no resuelto |
| `.pld/.plw` | sí | sí | experimental; PLD/PLW RE2 con MD1 + TIM interno cuando existe |
| `.md1` | sí | sí | experimental; mesh crudo RE2-style |
| `.md2` | sí | sí | experimental; mesh crudo RE3-style |

## Pipeline recomendado

```text
RE native / OBJ
      ↓
grc_load_model()
      ↓
grc_model + pieces + materials
      ↓
grc_model_validate() / grc_model_prepare()
      ↓
engine/editor/visor
      ↓
grc_save_model()
      ↓
GFM / OBJ / TMD / MD1 / MD2 / EMD / PLD / PLW
```

## API mínima

```c
grc_model model;
grc_options opt;
int rc;

grc_options_default(&opt);
opt.scale = 1.0f;

grc_model_init(&model);
rc = grc_load_model("input.obj", &model, &opt);
if (rc == GRC_OK) {
    opt.flags = GRC_CLEAN_DROP_INVALID |
                GRC_CLEAN_DROP_DEGENERATE |
                GRC_CLEAN_GENERATE_NORMALS |
                GRC_CLEAN_PSX_QUANTIZE;
    grc_model_prepare(&model, &opt);
    opt.native_game_hint = GRC_GAME_RE2;
    grc_save_model("output.emd", &model, &opt);
}
grc_model_free(&model);
```

## Avisos importantes

- Usa copias legales propias del juego; el proyecto no trae assets.
- RE3 puede tener vertex binding/skinning específico. El codec conserva y exporta malla base, pero no reconstruye binding real de personaje todavía.
- Animación, armature y reempaque retail perfecto quedan como siguientes fases.
- Para editor/engine, usa `.gfm` como cache/interchange principal y exporta nativo solo al final.

## Licencia

MIT. Ver `LICENSE`.
