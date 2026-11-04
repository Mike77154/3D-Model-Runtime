# hl1modelib — GoldSrc timeline & quaternion regression (C89)

**Cambio acumulativo, aditivo.** Conserva los módulos del ZIP `hl1modelib_real_corpus_firetest_C89.zip` y agrega `hlfp_timeline.h/.c`, `test_hlfp_timeline.c` y `corpus_probe_timeline.c`. No incluye corpus ni redistribuye activos de videojuegos.

## Cambios justificados por SDK y corpus

1. `hlfp_blend.c`: las secuencias con `STUDIO_LOOPING` recorren **`numframes-1`** fotogramas efectivos, acorde con `CStudioModelRenderer::StudioEstimateFrame` de Valve (`halflife-master/cl_dll/StudioModelRenderer.cpp`), no `numframes`. Eso alinea la ruta Euler/poses con la ruta de cuaterniones y evita un fotograma de demora al cerrar el ciclo.
2. `hlfp_ctrlquat.c`: normalización de ciclo por módulo sin bucles de resta repetida. **Corrección de lectura fuera de rango** en una secuencia de dos fotogramas de `scientist.mdl` de Cold Blood: al llegar al último fotograma no se intenta obtener el siguiente valor comprimido para interpolar con fracción cero. Este caso fallaba en el códec anterior y ahora pasa.
3. `hlfp_blend.c`, `hlfp_ctrlquat.c`: cálculo de límite de fotogramas 16.16 con multiplicación comprobable, evitando desplazamientos con riesgo de desbordamiento. Tanto la ruta Euler como la ruta quaternion rechazan recuentos no representables.
4. `hlfp_timeline.h/.c`: módulo nuevo de eventos de secuencia GoldSrc sin heap, VFS ni operaciones en coma flotante. Convierte segundos 16.16 a fotogramas y entrega **eventos en orden temporal**, soportando avance, retroceso, bucles y múltiples vueltas. El intervalo es `(inicio, fin]` en avance y `[fin, inicio)` en retroceso. La tabla de eventos se obtiene de `mdl->events` siguiendo su orden de secuencia, sin alterar las estructuras públicas preexistentes.
5. Pruebas sintéticas para intervalos de eventos, cambios de vuelta, reversa, capacidad de resultados, materialización de tiempo y claves duplicadas. `corpus_probe_timeline.c` prueba tres instantes fraccionarios por animación, incluyendo el instante posterior a la última clave de una secuencia no cíclica, más igualdad exacta de poses en cierre de ciclo para Euler/quaternion.

## Resultados reales

| Criterio | Resultado |
|---|---:|
| MDL principales (`IDST`) leídos mediante bundle loader | 834 / 834 |
| Secuencias examinadas | 7,086 |
| Muestras de pose tradicional | 20,262 correctas, 0 errores |
| Muestras de pose cuaternión/controller | 20,262 correctas, 0 errores |
| Pruebas de consulta de eventos | 7,086 correctas, 0 errores |
| Eventos emitidos en la ventana temporal de prueba | 1,125 |
| Comparaciones de cierre de ciclo en ambas rutas | 2,695 secuencias, 0 discrepancias |
| Caso regresión `scientist.mdl` | corregido, 0 errores |

Todos los resultados están en `TIMELINE_CORPUS_RESULTS.csv`. Las poses se **decodificaron**, pero no se ha medido visualmente cada frame ni se ha demostrado paridad del render completo con GoldSrc. Las consultas de eventos validaron un intervalo corto por secuencia y las pruebas sintéticas ejercitaron los bucles, no todas las combinaciones de eventos y velocidades posibles.

## Compilación y pruebas

```sh
make clean
make CFLAGS='-std=c89 -Wall -Wextra -Werror -pedantic -O2' all test_output.txt bundle_probe corpus_probe_timeline
./test_hlfp_timeline
./corpus_probe_timeline '/ruta/modelo.mdl'
```

MinGW32: en MSYS2/MINGW32 ejecutar `make` con `CC=gcc AR=ar`. El paquete fue compilado y probado en GCC/Linux con los flags anteriores; **no** se ejecutó en Windows/MinGW32.

## Integración del nuevo módulo

```c
#include "hlfp_timeline.h"
/* mdl ya está cargado por hlfp_bundle_load() */
static hlfp_timeline_hit hits[32];
static hlfp_error err;
int n, rc, i;
hlfp_fixed last_tick = 0;
hlfp_fixed tick = HLFP_FIXED_ONE / 2; /* medio fotograma */
rc = hlfp_timeline_collect_frames(&mdl, seq_id, last_tick, tick,
                                  hits, 32, &n, &err);
if (rc == HLFP_OK) {
    for (i = 0; i < n; ++i) {
        /* snd/vfx/attack se interpretan en el engine; el codec NO los ejecuta */
        const hlfp_mdl_event *ev = &mdl.events[hits[i].event_index];
        /* dispatch_to_engine(ev->event, ev->options); */
        (void)ev;
    }
}
```

**Contrato temporal:** posiciones absolutas en fotogramas 16.16, no `frame modulo period`. Mantén un reloj continuo aunque el modelo haga loop. Para detectar eventos al inicio de una animación, empieza en `-HLFP_FIXED_ONE`, pues el límite inicial es exclusivo. Un evento en el último fotograma de una secuencia looping coincide en tiempo con el evento del fotograma cero del siguiente ciclo: el API conserva ambos, en el orden de su tabla original.

**Concesiones:** no se corrigen diferencias de iluminación, chrome/fullbright/masked textures, export roundtrip ni scripts QC; siguen siendo frentes para iteraciones posteriores. El modelo debe permanecer en memoria mientras se usan sus eventos.
