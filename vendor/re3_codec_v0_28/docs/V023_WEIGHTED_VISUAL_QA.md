# RE3 Codec v0.23 — comprobación visual y aritmética de morphs ponderados

## Objetivo

Verificar visualmente la reconstrucción del morph PC de `ResidentEvil3.exe` recuperada en v0.22 por Ghidra y `objdump`. El problema detectado: v0.22 contenía `re3c_morph_blend_local_vertex` y `re3c_animated_body_vertex_morph_blend`, pero **el visor `re3emd_anim_preview` no llamaba esas funciones** y mantenía la lógica de v0.21.

## Modificaciones

- Se añade al visor un último parámetro opcional `pc:INDICE:PESO_Q12` con pesos 0..4096 y `pc-pulse:INDICE` con una progresión triangular ilustrativa. `INDICE` designa el canal de delta *dentro de cada pista de morph*, no una articulación.
- Se aplica el *morph con base absoluta binary01 frame0 + sumas de desplazamientos Q12* **antes** del skinning y de las matrices esqueléticas ya existentes.
- En las capturas comparativas de peso fijo, se calculan límites de cámara **comunes para todos los pesos 0/1024/2048/3072/4096** dentro del mismo clip, evitando ocultar las deformaciones mediante autozoom.
- Modo opt-in y reversible; no cambia el renderizador normal ni altera archivos comerciales.
- `re3emd_weighted_morph_audit` recorre todos los vértices morph y valida con aritmética independiente cinco pesos por canal y una mezcla de dos canales cuando existan. Tiene objetivo `make test-weighted-corpus` y compilación MinGW32 preparada.

## Datos del corpus (100 EMD, dos carpetas EMD/EMD08)

| Métrica | Valor |
|---|---:|
| EMD auditados | 100/100 |
| Modelos con morphs | 12 |
| Pistas morph | 50 |
| Vértices con morph contabilizados | 2,227 |
| Vértices donde binary01 frame0 difiere de MD2 | 256 |
| Verificaciones numéricas de un canal | 33,530 |
| Verificaciones de mezcla de dos canales | 1,540 |
| Desbordamientos en las pruebas | 0 |
| Discrepancias respecto a aritmética Q12 esperada | 0 |

Detalles específicos:

- `EM30`: 414 vértices con morph; **48** distintos en la posición inicial; 5,270 verificaciones de un canal; 320 de mezcla de dos canales.
- `EM33`: iguales cantidades que `EM30`.
- `EM36`: 258 vértices con morph; **34** distintos en la posición inicial; 4,920 verificaciones de un canal; 186 de mezcla de dos canales.

## Conclusión visual

- Grave Digger `EM30` muestra cambios graduales de forma entre 0 y 100 % manteniendo la continuidad aparente de su cuerpo principal en los fotogramas examinados.
- `EM33` también conserva su silueta, aunque los cambios en el ángulo elegido son discretos.
- Némesis sin camisa `EM36` presenta cambios graduales más sutiles en la región superior; no se aprecia una separación catastrófica en esta comparación.
- Los GIF `*_fixed_pose_weight_cycle.gif` mantienen **la misma pose y encuadre**, variando solamente el primer canal morph. Los GIF `*_weighted_pulse.gif` combinan además las poses reales del clip 4 con un pulso de peso **inventado como prueba**, no confirmado como curva de Capcom.
- El CSV `render_pixel_differences.csv` mide cambio de imagen respecto al 0 %; **no** es una medida de fidelidad al juego.

## Calidad

- GCC `-std=c89 -pedantic -Wall -Wextra -Werror -O2`: compilación y suite sintética **PASS**.
- Clang con las mismas restricciones: compilación y suite sintética **PASS**.
- `make test-weighted-corpus`: 100/100 **PASS**.
- Visor `EM30` con Clang UBSan: **6 fotogramas, sin avisos en esa ejecución**.
- No se probó aquí una compilación nativa Win32 con MinGW (script preparado para Windows).

## Reproducción

```
make
mkdir -p frames
./re3emd_anim_preview EM30.EMD EM30.TIM frames 0 4 20 quarter texture model main rest no-map - native pc-pulse:0
./re3emd_weighted_morph_audit EM30.EMD
```

Para Windows: ejecutar `build_mingw.bat`, después `demo_weighted_re3.bat EM30.EMD EM30.TIM resultados 4`.

## Alcance y limitaciones

La rutina aritmética corresponde a instrucciones de RE3 PC documentadas en v0.22 (`0x00438260` / Mercenarios `0x004382B0`), **pero los pesos concretos y su timing aquí son artificiales**. Todavía no se validó equivalencia visual exacta con el renderer original, ni la selección de geometrías opcionales, ni la correspondencia de bancos de daño. Además, el códec devuelve `OVERFLOW` en vez de emular el posible wrap i16 del x86. La mejora comprobada es **capacidad de renderizar morphs ponderados con transiciones correctas y un encuadre justo**, no el gameplay original reconstruido.

Ningún binario comercial, EMD/TIM ni corpus está incluido en las entregas.
