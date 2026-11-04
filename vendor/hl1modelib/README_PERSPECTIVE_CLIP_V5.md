# hl1modelib — GoldSrc PerspectiveClip C89 v5

**Version acumulativa del codec HL1/GoldSrc.** Se conserva la interfaz ortografica de v4 por defecto, ademas de todos los archivos fuente de v4 y anteriores. No se elimina ningun codec.

## Novedades

1. `hlfp_perspective89.c` / `hlfp_perspective89.h`: camara fija 16.16 con vectores `right`, `up`, `forward`, posicion `eye`, `focal` y plano cercano `near_z`.
2. Clipping de triangulos contra el plano cercano en espacio de camara (Sutherland-Hodgman para una cara; hasta cuatro vertices, triangulacion en abanico).
3. Proyeccion despues del clipping; se evita dividir por profundidad cero o negativa.
4. UV corregidas por perspectiva con pesos baricentricos proporcionados a `focal/z`. El Gouraud suavizado tambien usa pesos corregidos.
5. Cuando se activa la perspectiva, la cola de transparencia de v3 ordena sus elementos usando la distancia de camara, de lejos a cerca.
6. Prueba de regresion del rasterizador: un triangulo parcialmente recortado, totalmente detras del plano cercano, vertices a dos profundidades, comprobacion de UV a misma posicion de pantalla y fallback ortografico.
7. Demos renderizando modelos reales de Resident Evil: Cold Blood (`PREVIEWS_V5`).

## Configuracion

Incluir `hlfp_softmdl_plus.h` y configurar una camara opcional **de vida util superior a la llamada de render**:

```c
hlfp_soft2_options opt;
hlfp_p89_camera view;
hlfp_soft2_options_init(&opt);
hlfp_p89_camera_init(&view);
view.eye.z = 256 * HLFP_FIXED_ONE;
view.forward.z = -HLFP_FIXED_ONE;
view.focal = 256 * HLFP_FIXED_ONE;
view.near_z = 8 * HLFP_FIXED_ONE;
opt.perspective = &view;  /* NULL: modo ortografico de v4 */
```

El sistema sigue usando el `hlfp_soft_camera` original para la escala en pantalla, desplazamiento central y sentido del eje Y.

Esta nueva ruta es **optativa**. La estructura `hlfp_soft2_options` crecio un campo de puntero, por lo que las aplicaciones deben **recompilarse** al pasar de v4 a v5.

## Compilacion y pruebas ejecutadas

```sh
make clean
make -j4 CFLAGS='-std=c89 -Wall -Wextra -Werror -pedantic -O2' all corpus_probe_v5
./test_hlfp_v5
./test_hlfp_v4
./corpus_probe_v5 /ruta/a/zombie.mdl tiny
./demo_render_goldsrc_v5 /ruta/a/zombie.mdl salida 0 0 v5
```

Entorno de prueba: **GCC/Linux**, C89 estricto. Resultado: **11/11** ejecutables de prueba antiguos y nuevos (archivos `BUILD_V5_STRICT_C89.log`, `TEST_V5_SUITE.log`), **37** aserciones en `test_hlfp_v5` y **822/822** modelos sin errores de ejecucion en `corpus_probe_v5` (tabla `REPORT_V5_CORPUS_ALL.csv`). La prueba masiva es una prueba de humo diminuta; **no equivale** a calidad, fidelidad ni visibilidad de todas las mallas. Los PNG son renders completos realizados con modelos de Cold Blood.

## Limitaciones declaradas

- No se implementa todavia clipping completo por los planos izquierda/derecha/arriba/abajo. Los vertices que proyectan mas alla de +/-4096 pixeles de centro de pantalla se descartan para proteger aritmetica de 32 bits. Es una salvaguarda, no una proyeccion ideal de poligonos enormes.
- El rasterizador de software tiene un enfoque aproximado de GoldSrc; la comparacion pixel por pixel con GoldSrc original no esta validada.
- Los triangulos translucidos se ordenan por profundidad media; intersecciones o casos de sorting complejo todavia pueden tener artefactos.
- Las coordenadas UV interpoladas quedan cuantizadas por el esquema de enteros, coherente con la restriccion C89/16.16; hay que verificar los bordes finos en altas resoluciones.
- No hay prueba local de ejecucion en Windows/MinGW32: el cross-compiler i686-w64-mingw32-gcc no esta instalado en este entorno.

## Referencia original

Half-Life SDK, Valve, `utils/mdlviewer/studio_render.cpp`: https://github.com/ValveSoftware/halflife/blob/master/utils/mdlviewer/studio_render.cpp. El visor usa el pipeline de OpenGL para la proyeccion; esta v5 incorpora un camino de **software fijo** para nuestros motores, sin afirmar que sea una copia exacta del renderer OpenGL.

## Organizacion

- `hlfp_perspective89.[ch]` — nuevo vendor independiente.
- `hlfp_softmdl_plus.[ch]` — puente opt-in a la ruta de proyeccion en v5, sin eliminar la vieja.
- `test_hlfp_v5.c` — casos de borde de clipping, perspectiva y UV.
- `corpus_probe_v5.c` y `run_v5_corpus.py` — regresion real.
- `demo_render_goldsrc_v5.c` — prueba visual, usa tambien v4.
- `PREVIEWS_V5/HL1MODELlib_v5_PERSPECTIVE_CONTACT.png` — comparacion visual.
