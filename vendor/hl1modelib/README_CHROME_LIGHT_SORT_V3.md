# hl1modelib — GoldSrc Chrome / Light / Transparency v3 (C89)

Actualización **acumulativa** sobre `hl1modelib_GoldSrc_MaterialsMotion_C89_v2_FULL_SOURCE.zip`. No elimina los módulos viejos ni su API fuente; los nuevos campos del `hlfp_soft2_options` **sí cambian el ABI**: recompila bibliotecas y engines consumidores. Sólo código fuente, informes y previews, NO incluye modelos de Valve/mods.

## Cambios

| Módulo | Novedades |
|---|---|
| `hlfp_material89.c/.h` | Nuevas `hlfp_mat89_chrome_uv_view` (reflexión dependiente del observador), `hlfp_mat89_directional` (ambiente + difusa, respetando FULLBRIGHT/FLATSHADE). |
| `hlfp_transparency89.c/.h` | **Nuevo**. Cola externa `hlfp_trans89_queue`, `push`, `sort` heapsort estable por profundidad media y ordinal. Un solo orden sobre triángulos de todos los bodyparts y meshes seleccionados. |
| `hlfp_softmdl_plus.c/.h` | Nuevas opciones opt-in `chrome_view_dependent`, `chrome_eye`, `enable_directional_light`, `light_direction`, `ambient_8`, `diffuse_8`. API de composición `hlfp_soft3_render_mdl_sequence_subframe_textured` con cola provista por engine. |
| `test_hlfp_v3.c` | Prueba de chrome cambiado por cámara, intensidad y FULLBRIGHT, cola y overflow, opciones antiguas preservadas. |
| `corpus_probe_v3.c` | Comprobador que ejercita la ruta v3 con iluminación + vista + ordenamiento, sobre modelos reales con textura/animación. |
| `demo_render_goldsrc_v3.c` | Render GoldSrc de pantalla/poses desde dos posiciones de observador; emite TGA que pueden compararse. El ejemplo usa el renderer por mesh para mostrar chrome/luz, y la prueba corpus ejerce el compositor global. |

## Resultados reproducibles

- Compilación: GCC Linux `-std=c89 -Wall -Wextra -Werror -pedantic -O2` para biblioteca y objetivos `all`.
- Regresiones heredadas: `test_hlfp`, `test_hlfp_goldsrc`, `test_studiohalflife`, `test_studiohalflife_plus`, `test_studiohalflife_ultra`, `test_hlfp_corpusfix`, `test_hlfp_timeline`, `test_hlfp_material89` pasan.
- Nueva suite: `test_hlfp_v3` = 0 fallos. Verifica que chrome con una normal realista pasa de coordenadas UV (19,24) a (31,24) al mover el ojo.
- Corpus completo: **822/822 modelos principales** del paquete Half-Life + Resident Evil Cold Blood: *smoke test* 12×12 de primera secuencia, primer subframe, con opciones v3 activas; **0 fallos de ejecución** (`REPORT_V3_CORPUS_ALL.csv`).
- De ellos, **29 modelos** hicieron uso de la cola de triángulos translúcidos, **13,721 triángulos acumulados**, máximo **8,120 triángulos en un solo modelo** (v_deagle). Cola de ensayo de **16,384** elementos provista estáticamente por el test. La cola también incluye elementos aditivos (el orden no altera la suma salvo saturación).
- Tres modelos de Cold Blood con chrome: llave, Desert Eagle y máquina expendedora; entre dos observadores (con pose/cámara/proyección constantes) difieren **7,044**, **1,131** y **1,655** píxeles RGB respectivamente. **No es métrica de paridad con GoldSrc**. Imagen en `PREVIEWS_V3/HL1MODELlib_v3_CHROME_CONTACT.png`.
- No se han probado builds nativos MinGW32 ni equivalencia visual píxel a píxel con GoldSrc vivo.

## Ejemplo C89 (nueva composición)

```c
#include "hlfp_softmdl_plus.h"
/* Cola y scratch estáticos, definidos por el caller: no malloc/free. */
static hlfp_trans89_triangle triangles[16384];
static hlfp_trans89_queue queue;
static hlfp_soft_scratch scratch;

/* mdl, framebuffer fb, cam, err declarados e inicializados por el engine. */
void render_model(const hlfp_mdl *mdl, hlfp_soft_framebuffer *fb,
                  hlfp_soft_camera *cam, hlfp_error *err)
{
    hlfp_soft2_options options;
    int status;
    hlfp_soft2_options_init(&options);
    options.wire_overlay=HLFP_FALSE;
    options.skeleton_overlay=HLFP_FALSE;
    options.chrome_view_dependent=HLFP_TRUE;
    options.enable_directional_light=HLFP_TRUE;
    options.chrome_eye.x=0;
    options.chrome_eye.y=0;
    options.chrome_eye.z=128*HLFP_FIXED_ONE;
    hlfp_trans89_init(&queue,triangles,16384U);
    status=hlfp_soft3_render_mdl_sequence_subframe_textured(
       mdl,0,0,0,HLFP_FIXED_ONE/2,0,fb,cam,&options,&scratch,&queue,err);
    if (status!=HLFP_OK) { /* inspect err; no silent queue drop */ }
}
```

La cola es **caller-owned** y se reinicia en cada llamada; nunca la compartas concurrentemente entre motores/hilos. Al devolver error por capacidad, la imagen puede estar parcialmente escrita: borra framebuffer o renderiza de nuevo con más capacidad.

## Compilación y pruebas

```sh
make clean
make -j4 CC=gcc AR=ar CFLAGS='-std=c89 -Wall -Wextra -Werror -pedantic -O2' all corpus_probe_v3
./test_hlfp_v3
./test_hlfp_material89
./corpus_probe_v3 '/ruta/modelo.mdl' tiny
./demo_render_goldsrc_v3 '/ruta/modelo_con_chrome.mdl' /tmp/view 0 0.5
```

El compilador no se ejecutó aquí en Windows/MinGW32. Compila localmente en el entorno MinGW32 del usuario para validarlo allí. No se utilizan `malloc/free`, aritmética de coma flotante ni enteros explícitos de 64 bits en los **nuevos módulos**.

## Lo que sigue pendiente

1. Chrome **idéntico** a GoldSrc: actualmente se usa vector observador→vértice y base aproximada L1; el renderer original calcula los ejes por hueso, usando rotación inversa de su matriz. Comparación de fotogramas contra GoldSrc real aún pendiente.
2. Iluminación GoldSrc idéntica: el nuevo módulo suma ambiente y difusa por **normal media del triángulo**; falta la iluminación por vértice con hemisferio Lambert de GoldSrc, luces locales y suavizado correcto.
3. Cola ordenada por z media (orden global) no puede resolver intersecciones de triángulos ni transparencia por-píxel; no ofrece OIT (order-independent transparency).
4. El raster ortográfico conserva UV afín, sin proyección perspectiva, clipping near plane y sin filtrado de textura de GoldSrc.
5. Capacidad de cola a dimensionar por caller; algunos modelos muy grandes podrían superarla. Los 822 ensayos de 12×12 validan flujo y ejecución, no fidelidad fotograma a fotograma ni todas las animaciones.

Referencias de contraste: Valve Half-Life SDK `utils/mdlviewer/studio_render.cpp` (funciones `StudioModel::Chrome`, `StudioModel::Lighting`), `halflife/engine/studio.h`; Xash3D-FWGS aportado por el usuario.
