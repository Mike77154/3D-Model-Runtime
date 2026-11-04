# Giffany MS3D Codec C89 v1.7 — SLERP, Crossfades & Transparency

## Entrega

Versión aditiva basada en la v1.6. **No se eliminaron** las rutas de importación, matrices de skinning, normalización/smooth shading, UV por esquina, referencias a material, ni exportadores OBJ/MTL.

## Cambios ejecutados

| Archivo | Función |
|---|---|
| `gif_ms3d_quat.[ch]` | SLERP esférico sobre cuaterniones Q16.16 mediante CORDIC y `acos` por bisección. En orientaciones casi paralelas usa NLERP por estabilidad. |
| `gif_ms3d_skin.[ch]` | Nuevo selector SLERP; `ms3d_skin_evaluate_blend` interpola dos muestras locales de la misma jerarquía antes del skinning. |
| `gif_ms3d_player.[ch]` | `ms3d_player_blend`, `blend_clear`, `crossfade_to_range`; transiciones cronometradas a intervalos de frames/tiempo elegidos por el engine. |
| `gif_ms3d_render.[ch]` | Adaptador de estado de transparencia: opaco, alfa por umbral y mezcla alfa; orden de triángulos transparentes por profundidad en el modo 2. |
| `ms3d_player_stream_obj.c` | Opción `--slerp` además de `--nlerp` y `--euler`; UV/material/MTL intactos. |
| `test_ms3d_v17.c` | Pruebas sintéticas nuevas de interpolación esférica, rotación, UV/material, crossfade, alfa y orden de dibujo. |
| `ms3d_v17_corpus_probe.c` | Verificador directo de todas las mallas; 3 poses con SLERP más blend adicional cuando hay duración. |

## Pruebas realizadas

- Compilación estricta y tests **GCC** y **Clang**: PASÓ (`-std=c89 -O2 -Wall -Wextra -Werror -pedantic`).
- Corpus Night Of The Zombies: **157/157 modelos** aprobados. Los 3 samples SLERP y la prueba opcional de blend mantienen número de triángulos, UV por esquina, material y referencias de grupo. Datos completos en `QA_corpus_157_v1_7.json`.
- ZMB: 735 triángulos, 1 material, 6 fotogramas originales texturizados (0 a 1750 ms).
- ZMB_HS: 685 triángulos, 2 materiales, 6 fotogramas de prueba.
- Numérica: interpolación 0°→120° al 25% se aproxima a 30°; la prueba comprueba que NO se confunda con NLERP; incluye quaterniones antipodales y endpoints.
- Transición programada: tiempo A en rango 0–1000 ms hacia B en 1000–2000 ms durante 400 ms, con cambio efectivo al rango B después del blend.

## Cómo reproducir

```sh
./build_msys2_mingw32.sh
./examples/ms3d_player_stream_obj data/zmb.ms3d output/zmb 6 350 --slerp --smooth-weld --texture-root data
./examples/ms3d_v17_corpus_probe data/zmb.ms3d
```

El script MinGW32 no se ejecutó en un equipo Windows real. La librería opera sin dependencias gráficas, pero **el códec aún no carga los bytes de las texturas al GPU ni implementa un shader**. El renderer debe procesar las rutas `texture` y `alphamap`, las UV nativas top-left y los hints alpha+draw order.

La clasificación de triángulos con mapas alfa se hace conservadoramente, y el orden por centroide no elimina artefactos con polígonos transparentes intersecados. El `mode` particular de cada material no se interpreta como una modalidad de translucidez porque la especificación anota que está obsoleto/no usado.

SLERP y blending no prueban automáticamente paridad con el software MilkShape; para garantizarla necesitamos contrastar fotogramas exactos de referencia.

## Referencias para continuar

- https://sappersblog.blogspot.com/2014/08/milkshape-3d-185-ms3d-file-format.html
- https://gist.github.com/sapper-trle/db1dc6670ec0fa733d7d
- https://paulbourke.org/dataformats/ms3d/index.html

## Siguiente fase recomendada

Transiciones aditivas y enmascaradas por hueso, importación de definiciones de clips de un manifiesto (externo al MS3D), y backend OpenGL/Win32 con rasterización real y decodificación de mapas alfa; después validación contra MilkShape, incluyendo modelos con huecos/costuras reales.
