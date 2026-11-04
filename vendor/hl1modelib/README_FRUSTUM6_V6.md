# hl1modelib / GoldSrc — Full Frustum Clipping C89 v6

Version acumulativa de `hl1modelib` derivada de `GoldSrc_PerspectiveClip_C89_v5`.
**Se mantienen todos los módulos de v5 y anteriores.** No se sustituyen los codecs MDL/SMD.

## Qué se ha solucionado

En la v5 un vértice que proyectaba fuera del rango fijo de +/-4096 píxeles hacía que se descartase **todo el triángulo**, incluso cuando su interior cruzaba la pantalla. Esto generaba agujeros al acercar mucho la cámara a una malla o al mirar geometría enorme.

La v6 añade el vendor `hlfp_frustum89.[ch]`, que realiza clipping de Sutherland–Hodgman en **espacio de cámara** contra seis semiespacios: cerca, lejos, izquierda, derecha, abajo y arriba. Los planos laterales se derivan de la cámara 16.16 y del viewport existente (`hlfp_soft_camera`), incluyendo `flip_y`. El resultado tiene entre 0 y 9 vértices, triangulados en abanico con el winding original. UV, posición y normal se interpolan en el corte; asociación discreta de huesos se elige en función del extremo más cercano. Proyección y UV perspectivas de la v5 siguen operando sobre los vértices recortados.

También se integra en el esqueleto (clipping de segmentos) y en `hlfp_soft3_render_mdl_sequence_subframe_textured`: los triángulos translúcidos completamente invisibles no consumen slots, y la prioridad de sorting se obtiene de la profundidad media de la porción visible. La cola sigue usando memoria proporcionada por el llamador; **no usa `malloc`/`free`**.

## API optativa

Configurar `hlfp_soft2_options` (después de `hlfp_soft2_options_init`), además de la cámara perspectiva de la v5:

```c
hlfp_soft2_options opt;
hlfp_p89_camera view;
hlfp_soft2_options_init(&opt);
hlfp_p89_camera_init(&view);
view.eye.z = 256 * HLFP_FIXED_ONE;
view.forward.z = -HLFP_FIXED_ONE;
view.focal = 256 * HLFP_FIXED_ONE;
view.near_z = 8 * HLFP_FIXED_ONE;
opt.perspective = &view;
opt.full_frustum_clipping = HLFP_TRUE; /* v6 opt-in */
opt.perspective_far_z = 8192 * HLFP_FIXED_ONE;
```

- `opt.full_frustum_clipping = HLFP_FALSE` (por defecto) conserva exactamente la ruta de proyección cercana de v5.
- `opt.perspective_far_z`: distancia máxima positiva **en espacio de cámara**, debe superar `view.near_z`. Por defecto 8192 unidades, sólo cuando la ruta v6 está habilitada.
- `opt.perspective == NULL` sigue usando la ortográfica de v4.
- El frustum se prepara **una vez por malla/pase** y no una vez por triángulo.
- La nueva ruta asume `screen.scale > 0`. Viewports hasta 8192×8192 (con restricciones de desplazamiento de cámara), únicamente coordenadas 16.16 y aritmética que es válida en targets `int`/`long` de 32 bits.
- Las estructuras `hlfp_soft2_options` ya habían cambiado desde versiones anteriores y se amplían otra vez en v6. **Recompilar todos los consumidores de esta estructura.** No mezclar binarios v5 y v6.

## Compilación y pruebas

```sh
make clean
make -j4 CC=gcc AR=ar CFLAGS='-std=c89 -Wall -Wextra -Werror -pedantic -O2' all corpus_probe_v6
./test_hlfp_v6
./test_hlfp_v6_stress
./demo_frustum_v6 PREVIEWS_V6/old.ppm PREVIEWS_V6/new.ppm
./corpus_probe_v6 /ruta/a/tu/modelo.mdl tiny
./demo_render_goldsrc_v6 /ruta/a/modelo.mdl salida 0 0 v6
```

En un MSYS2/MinGW32 instalado con GCC y `make`, cambiar a esa carpeta y ejecutar el mismo comando. La compatibilidad **no se ha comprobado mediante un compilador MinGW en el entorno de esta prueba**.

## Resultados ejecutados

- **Compilación limpia GCC/Linux:** `-std=c89 -Wall -Wextra -Werror -pedantic -O2`, satisfactoria.
- **13 ejecutables de tests:** aprobados (`test_hlfp` a `test_hlfp_v6_stress`, incluidas las pruebas heredadas).
- **62 comprobaciones v6** sobre planos, segmento de esqueleto, polygon clipping, materiales sintéticos y compatibilidad v5.
- **20,000 triángulos sintéticos deterministas:** 3,338 polígonos visibles, 16,662 culls; ninguna salida fuera de los seis planos en la tolerancia definida en el test.
- **822 MDL reales GoldSrc:** prueba de humo diminuta de **12×12** con ruta v6 activada, **822 sin fallo de ejecución** (archivo `REPORT_V6_CORPUS_ALL.csv`). El corpus usado fue el mismo de la v5; `run_v6_corpus.py` contiene el runner y las rutas locales de prueba.
- **Diferencia reproducible:** la v5 descartó un triángulo sintético completo; la v6 conservó 40,164 píxeles a 256×256 en el demo `demo_frustum_v6.c`. También hay prueba a 96×96 en `test_hlfp_v6` (6,865 píxeles recuperados).
- **Zombi real de Cold Blood:** se renderizó en v5 y v6 con idéntica cámara, sin diferencia de píxeles en un caso donde no era necesario recortar. Esto es una regresión de compatibilidad, no una evaluación de fidelidad contra GoldSrc original.

## Recursos nuevos

- `hlfp_frustum89.h`, `hlfp_frustum89.c`: API reusable vendor para geometría y segmentos.
- `test_hlfp_v6.c`: pruebas funcionales, incluidas las seis caras y un triángulo que desaparecía en v5.
- `test_hlfp_v6_stress.c`: pruebas deterministas de 20k triángulos sin heap.
- `demo_frustum_v6.c`: comparación de render real v5/v6 (PPM).
- `demo_render_goldsrc_v6.c`: cámara opcional v6 sobre modelos MDL.
- `corpus_probe_v6.c`, `run_v6_corpus.py`: corpus real y pruebas de humo.
- `PREVIEWS_V6/FRUSTUM_V5_V6_REAL_RENDERER_COMPARISON.png`: caso antes/después renderizado por el código C89.
- `PREVIEWS_V6/HL1MODELlib_v6_REAL_ZOMBIE_COMPARISON.png`: zombi real de GoldSrc sin pérdida de compatibilidad.
- `BUILD_V6_STRICT_C89.log`, `TEST_V6_SUITE.log`, `REPORT_V6_CORPUS_ALL.csv`: evidencia de pruebas.

## Limitaciones

1. Las mallas translúcidas todavía se ordenan por profundidad media, no por BSP ni orden perfecto en triángulos que se intersecan.
2. Render software 16.16 cuantizado: bordes de recorte, UV y subpíxeles no reproducen píxel a píxel OpenGL/GoldSrc.
3. No hay clipping general de otras primitivas aparte de triángulos y segmentos en la API nueva.
4. Los planos son semiespacios de una cámara con vectores `right/up/forward` que debe configurar correctamente el motor; no se valida ortonormalidad de la matriz de cámara.
5. En corpus masivo se prueba sólo que la ruta ejecuta correctamente; no significa que todas las texturas, animaciones, variantes de bodygroup y cámaras hayan quedado validadas visualmente.
6. No hay ejecución validada en Windows/MSYS2/MinGW32 en esta fase.

## Política C89

El vendor `hlfp_frustum89` no incorpora `malloc`, `free`, `float`, `double` ni `long long`. Usa almacenamiento automático acotado a 9 vértices y aritmética fixed-point 16.16 con comprobaciones de overflow, compatible con objetivos Win32 con `long` de 32 bits.
