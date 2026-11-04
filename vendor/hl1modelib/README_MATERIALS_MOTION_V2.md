# hl1modelib — GoldSrc Materials & Motion v2 (C89)

**Actualización acumulativa y aditiva del ZIP `hl1modelib_GoldSrc_Timeline_C89_vNext.zip`.** Mantiene los módulos originales y añade soporte de materiales en raster software, render multi-bodypart y un ejemplo de fotogramas fraccionarios. No incluye el corpus de modelos ni material propietario de juegos; únicamente código, resultados y capturas de prueba.

## Cambios de código

| Archivo | Trabajo realizado |
|---|---|
| `hlfp_material89.h/.c` | **Nuevo**, separa la interpretación de paletas, máscara índice 255, aproximación chrome por normales, clasificación opaco/translúcido, compositor RGB alfa y aditivo. Sin heap, flotantes o 64-bit explícitos. |
| `hlfp_softmdl_plus.h/.c` | **Ampliado**, flags por textura (masked, fullbright, flatshade, chrome, additive, alpha), dos pasadas de render (opaca y translúcida), Z sin escritura para superficies translúcidas, selector `body_value` y `skin_family`, recorrido de todos los bodyparts y sus meshes. |
| `hlfp_softmdl_plus.h/.c` | **Nueva API**, `hlfp_soft2_render_mdl_sequence_subframe_textured`: interpola matrices por cuaterniones con reloj en fotogramas 16.16, sin modificar APIs anteriores. |
| `demo_render_goldsrc_subframe.c` | **Nuevo ejemplo**, render de posiciones `0.0`, `1.5`, etc. La cámara se establece al fotograma cero para comparar instantes sin cambios automáticos de zoom. |
| `test_hlfp_material89.c` | **Nuevo**, regresión de transparencias, alpha, saturación aditiva, UV chrome, máscara, colores y opciones. |
| `corpus_probe_material89.c` | **Nuevo**, auditoría de banderas sobre modelos reales y smoke test opcional de render fraccionario completo con `tiny`. |
| `Makefile` | Añade los nuevos módulos, objetivos de pruebas y demos. |

### Interfaz del renderer

```
hlfp_soft2_options opt;
hlfp_soft2_options_init(&opt);
opt.body_value = 0;                  /* bodygroups GoldSrc */
opt.skin_family = 0;                 /* skin familiar GoldSrc */
opt.enable_chrome = HLFP_TRUE;
opt.enable_alpha = HLFP_TRUE;
opt.alpha_8 = 128;                   /* alpha constante: aproximación */
opt.enable_additive = HLFP_TRUE;
opt.additive_strength_8 = 255;
opt.draw_masked = HLFP_TRUE;         /* los píxeles 255 se omiten */
/* mdl, fb, cam, scratch ya preparados por el engine */
rc = hlfp_soft2_render_mdl_sequence_subframe_textured(
    &mdl, 0, 1, 0, HLFP_FIXED_ONE + HLFP_FIXED_ONE / 2,
    0, &fb, &cam, &opt, &scratch, &err);
```

El método `hlfp_soft2_render_mdl_sequence_frame_textured` sigue disponible con su firma antigua y se implementa sobre la función de subframes. `hlfp_soft2_draw_expanded_mesh_textured` conserva también la API existente.

## Mediciones reales

- **835 archivos `.mdl`** encontrados entre los corpus HL1 y Cold Blood, de ellos **822 modelos GoldSrc principales** admitidos, **12 archivos externos de texturas** (no son modelos independientes) y **un IDSQ de secuencias externo** (tampoco es un modelo independiente).
- En 822 modelos principales: **2,394 registros de textura**, **174 flags MASKED**, **64 CHROME**, **50 FLATSHADE** y **31 ADDITIVE**. **0 FULLBRIGHT y 0 ALPHA** observados: estos últimos sólo tienen validación sintética en esta iteración.
- **822 / 822** modelos principales pasan un *smoke test* de render fraccional muy pequeño (12 × 12); no es una prueba de paridad visual ni un render exhaustivo de todas las animaciones.
- **261 / 261** modelos de la submuestra de estrés (todos los que presentan flags especiales, más 80 modelos ordinarios de control) pasaron ese mismo tipo de prueba.
- 4 renders a **768 × 768** del zombie, Mr. X, Blue Herb y MagLite. Compilación estricta C89 y baterías anteriores de pruebas OK.
- Comparación visual del raster de 768 × 768 contra la versión anterior: **8,435 píxeles RGB distintos** en Blue Herb y **302,762** en MagLite. Son diferencias, **no una cifra de exactitud** con el motor de Valve.
- Muestra de movimiento del zombie (secuencia 1) en fotogramas **0, 1.5, 3, 4.5, 6, 7.5** con cámara constante. El GIF muestra seis muestras interpoladas, no la totalidad de una animación.
- Informes reproducibles en `REPORT_MATERIAL_FLAGS_CORPUS.csv` y `REPORT_RENDER_SMOKE_822.csv`.

## Compilación

En GCC/Linux se comprobó:

```sh
make clean
make -j4 CFLAGS='-std=c89 -Wall -Wextra -Werror -pedantic -O2' all test_output.txt corpus_probe_material89
./test_hlfp_material89
./corpus_probe_material89 '/ruta/modelo.mdl' tiny
./demo_render_goldsrc_subframe '/ruta/zombie.mdl' /tmp/zombie_pose 1 1.5
```

Para **MSYS2 MinGW32**, el código sigue sin malloc/free, float/double o tipos enteros 64-bit explícitos, pero **todavía no se probó en un entorno Windows real**. Si el compilador del usuario tiene GCC y make, se usa `make CC=gcc AR=ar ...`. Los ejemplos emiten TGA, no dependen de la GPU.

## Qué NO afirmamos

- El render no se ha comparado fotograma a fotograma con un **GoldSrc real ejecutándose**: no hay paridad visual demostrada.
- **CHROME** utiliza un mapeo por normales ortográfico aproximado. Falta recrear transformaciones view-dependent de GoldSrc y validar con cámaras animadas.
- **ALPHA** aplica el valor uniforme `alpha_8`; el archivo `.mdl` no incluye la opacidad de una entidad del nivel. 
- La solución de translúcidos usa dos pasadas globales y prueba de profundidad, pero **no ordena triángulos translúcidos de atrás hacia delante**. Algunas intersecciones y efectos complejos seguirán difiriendo.
- El rasterizador usa **proyección ortográfica + UV afines**, no filtrado y proyección idénticos al motor original; sobretriángulos/culling, gamma, iluminación de entorno y ciertas texturas no son exactos.
- El *smoke test* de 12×12 recorre estructuras y ramas de ejecución; **no valida fidelidad visual ni fotogramas completos de 822 modelos**.
- Las banderas completas fueron tomadas del encabezado `engine/studio.h` publicado por Valve y corroboradas con Xash3D y documentación práctica de GoldSrc.

## Fuentes técnicas consultadas

- https://github.com/ValveSoftware/halflife/blob/master/engine/studio.h
- https://www.the303.org/tutorials/gold_mdl.htm
- https://the303.org/tutorials/sven_fullbright.htm
- Incluidos en las entradas del usuario: `halflife-master.zip`, `xash3d-fwgs-master.zip`, `HalfLifeAssetManager-master.zip`, SDK y corpus HL1/Cold Blood.
