# HOD2 Character Codec — Phase10: PE real y referencias deformables

**Estado:** lector y auditor C89 nativo; NO se declara skinning GLB completo.
**Referencias:** ejecutables entregados por el usuario, bancos POL/TEX/MOT de las ediciones 1999 y 2000, y el proyecto `hotd2-re` (principalmente `src/game/skin.cpp`, `skin.h`, `world_st4_actors.cpp`). El proyecto de referencia es C++; este modulo nuevo esta implementado en C89. No se incluyen ejecutables ni bancos comerciales en el paquete.

## Ejecuciones reales

- EXE 1999: 1,699,840 bytes; SHA256 `c6b4116788b7f68c56860fb9cc8a94bf984e620907031e4bc3db43623dbe579a`.
- EXE 2000: 1,695,744 bytes; SHA256 `1d71265096661cf034568ff71184fcbbed0cd9aea0e5e2f041801c39a0ffce69`.
- 86/86 esqueletos reconocidos por version, 1,295 modelos referenciados, 0 referencias fallidas, 0 UV no finitas.
- Tabla `0x0052ED08` de 1999; en el perfil 2000: `0x0052DD08`; punteros a registros resueltos desde el PE real.
- 131 descriptores o entradas de partes por version: 103 con coincidencia completa EXE ↔ POL; 27 vacios (`NULL`), 1 ruta especial (`0x0041A020` en 1999 / `0x0041A0F0` en 2000).
- 2,486 referencias a registros de vertice de POL localizadas de 2,486, por cada version. Esta auditoria cuenta referencias, NO todos los vertices de los modelos ni demuestra deformacion durante MOT.
- Tipo 15, pieza 0: modelo 7156, 19 referencias y 9 reemplazos de vectores base generados por el lector `CharacterDrawBonePart`; CSV byte-identico entre ambas versiones.
- Tipo 23: la ruta especial es la cola articulada de 8 segmentos, modelos 8130..8137 y huesos 16..23. Las ocho referencias a modelos estan resueltas por ambos perfiles. La actual GLB no implementa el seguimiento dinamico de vertices de la cola.
- Tipo 15 exportado como personaje rigido texturizado, ambas versiones: 16 nodos, 15 mallas rigidas, 26 materiales, 8 texturas PNG embebidas. Los dos GLB son identicos byte a byte y son parseables con `trimesh` y validados por `glbcheck`. **No llevan animacion MOT ni deformacion abdominal.**

## Nuevos comandos

```
hod2c89 skinaudit 1999 "Hod2 - y99.exe" "hod2ye99/pol" skin99.csv
hod2c89 skinaudit 2000 "Hod2 - y2000.exe" "hotd2/pol" skin00.csv
hod2c89 skinmap 1999 "Hod2 - y99.exe" "hod2ye99/pol" 15 0 skinmap15.csv
hod2c89 tail23 1999 "Hod2 - y99.exe" tail23.csv
```

`skinaudit` audita todos los descriptores de tipos 0..85, localiza los registros de vertices y compara los offsets a partir de las dos bases POL candidatas. Un estado `full_match` significa que **cada referencia almacenada se encuentra** en un registro de vertice POL real, no que todos los vertices del modelo se deformen.

`skinmap` exporta las palabras binarias IEEE754 originales de los vectores de origen como hexadecimal, sin ninguna operacion `float`/`double`. `override=1` significa que esa fila se asocio con un vector fuente de deformacion; no es un frame MOT evaluado. Se valida que los offsets existan en los vertices referenciados del POL.

`tail23` identifica el descriptor de segmentos del tipo 23 y comprueba que cada modelo exista en el catalogo PE.

## Verificaciones

- `make check`: PASS (sinteticas previas).
- `gcc -std=c89 -pedantic -Wall -Wextra -Werror`: PASS.
- `clang -std=c89 -pedantic -Wall -Wextra -Werror`: PASS.
- Clang AddressSanitizer y UndefinedBehaviorSanitizer: pruebas reales `skinaudit 1999`, `skinmap 1999 tipo 15` y `tail23 2000` sin incidencias detectadas. Los CSV producidos son identicos a GCC.
- Clasificacion de partes y offsets en las dos ediciones: iguales.
- Compilacion nativa MSYS2 i686/Win32: NO se ha ejecutado en Windows aqui. Incluido `build_win32.bat` actualizado.

## Restricciones y trabajo pendiente

1. Reconstruir la transformacion de matrices del motor en el frame MOT con rig y poses: el esquema del EXE usa matrices y dos/cuatro grupos de deformacion, no simples pesos automaticos.
2. Exportar piezas deformables al GLB animado; actualmente el exportador es rigido. No debe marcarse abdomen/tail como skin completo.
3. Reconstruir la ruta especial de la cola tipo 23, incluida su deformacion segun segmento previo.
4. Resolver los tipos cuyo exportador rigido aborta aunque el auditor POL/PE confirme la referencia, y validar todos los GLB visualmente, incluidas costuras, abdomen y materiales por version.
5. No se han convertido uno a uno todos los scripts heredados Java/Python; los originales se conservan en el paquete FULL como referencia.
