# RE3 Codec v0.20 — auditoría de clips de criaturas

## Alcance y procedencia

Se utilizaron los EMD/TIM originales del corpus aportado por el usuario. El ZIP de código **no** redistribuye modelos ni texturas comerciales. Nombres de criaturas: identificación visual probable, no verificada con las tablas de entidades originales del juego.

- EM23: probable Drain Deimos (coloración parda/rojiza).
- EM28: probable Brain Sucker (coloración verde, rasgos faciales diferentes).
- EM30: Grave Digger, primera variante de textura.
- EM33: Grave Digger, segunda variante de textura.

## Auditoría exhaustiva de bancos EDD/EMR

| EMD | Geometría | Joints A | Clips A | Keys A | Clips damage | Keys damage | Keys decodificadas sin error |
|---|---|---:|---:|---:|---:|---:|---:|
| EM23 | 21 partes, 572 vértices, 645 caras | 21 | 32 | 1338 | 10 | 406 | 1744 |
| EM28 | 21 partes, 569 vértices, 677 caras | 21 | 36 | 1414 | 10 | 406 | 1820 |
| EM30 | 18 partes, 816 vértices, 1227 caras | 17 | 26 | 1865 | 1 | 40 | 1905 |
| EM33 | 18 partes, 816 vértices, 1227 caras | 17 | 14 | 709 | 1 | 40 | 749 |

TOTAL: 130 clips, 6218 keys decodificadas. 108 clips normales aptos para vista previa; 22 clips de daño decodifican las keys pero presentan bancos de 15 articulaciones que NO tienen correspondencia verificada con el cuerpo. NO se renderizan como si fueran correctos.

## Hallazgo de compatibilidad (EM23 / EM28)

Comparación de **cada pose decodificada**, no sólo del número de keys:
- Los primeros 32 clips emparejados tienen longitud idéntica.
- 1338/1338 poses tienen las mismas rotaciones (todas las articulaciones), velocidad/traslación y origin_y.
- EM28 contiene cuatro clips A adicionales: números 32–35.
- Esto demuestra reutilización de datos de poses entre ambas criaturas; no demuestra identidad de sus mallas, materiales, eventos ni timings del juego.

## Hallazgo de Grave Digger

EM30 y EM33: 816/816 posiciones de vértices iguales; 1227/1227 caras con la misma topología; 18/18 partes con la misma distribución de vértices/caras. Son variantes de un cuerpo geométrico compartido, con clips y TIM diferentes.

EM30 y EM33 contienen cuatro pistas morph (partes 1, 2, 3, 8). Prueba explícita del estado morph 1 sobre las cuatro pistas en EM30 produjo fragmentación severa y desplazamientos de piezas en el render. Esto NO debe aceptarse como transformación final válida sin más investigación: puede depender de reglas adicionales de morph, interpretación de las coordenadas o estados del juego. La vista sin morph es coherente durante la flexión del gusano.

## Clips completos comprobados

- EM23 banco A clip 4: 69/69 keys a BMP -> GIF.
- EM28 banco A clip 4: 69/69 keys a BMP -> GIF.
- EM28 banco A clip 32: 31/31 keys a BMP -> GIF (clip exclusivo frente a EM23).
- EM30 banco A clip 4: 104/104 keys a BMP -> GIF.
- EM33 banco A clip 13: 119/119 keys a BMP -> GIF.

Los GIF se produjeron con duración de fotograma arbitraria para facilitar la observación; no representan la tasa de fotogramas ni los triggers originales de Capcom. El visor C89 usa proyección sencilla y no emula píxel por píxel el hardware o los estados gráficos originales.

## Herramientas nuevas

`re3emd_clip_catalog` genera CSV completo con recuentos y validez de keys, y una comprobación de tres poses de skinning por clip compatible. No infiere semántica (andar, trepar, morir) solamente de un índice.

`re3emd_compare_clips` compara todos los campos de las poses en dos EMD y detecta reutilización exacta de animaciones.

Uso:

```
re3emd_clip_catalog EM23.EMD EM23_catalog.csv
re3emd_compare_clips EM23.EMD EM28.EMD
mkdir preview
re3emd_anim_preview EM30.EMD EM30.TIM preview 0 4 104 profile texture model main rest no-map
```

## Reparación adicional de build_mingw.bat

Corregida una concatenación accidental de dos comandos de compilación (`test_synthetic` y `test_visual_state`) y añadida la compilación de las dos herramientas de catálogo/comparación. La compilación MinGW32 no se ejecutó en este entorno porque no hay compilador cruzado Windows disponible.

## QA

- GCC y Clang: compilación en ISO C89 con `-std=c89 -pedantic -Wall -Wextra -Werror`.
- `make test`: batería sintética previa superada.
- UBSan: catálogo EM30 superado sin diagnósticos en la ejecución.
- Auditoría EMD: 130 clips / 6218 keys decodificadas con cero errores.
- 26 vistas previas muestreadas completadas sin errores de proceso.
- 5 GIFs de clip completo con todas sus keys producidos correctamente.

## Pendientes reales

1. Confirmar los ID `EM23`/`EM28` en las tablas de enemigos del ejecutable o scripts RDT.
2. Reconstruir jerarquía/retarget para los 22 clips de daño, sin asumir igualdad de índices.
3. Identificar por qué el estado morph 1 de Grave Digger produce geometría fragmentada; comparar con comportamiento original y estructura del Binary01.
4. Recuperar etiquetas semánticas de clips desde estados y lógica del ejecutable, no asignarlas arbitrariamente.
