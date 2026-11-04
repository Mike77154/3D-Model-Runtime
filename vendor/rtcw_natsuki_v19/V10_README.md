# RTCW Monster Codec v0.10 — Wolfanim Script Graph

**Objeto:** analizar offline `wolfanim.script` para localizar rutas de animaciones, sin ejecutar lógica de IA, condiciones, comandos de sonido ni cambios de estado. Los resultados enlazan nombres de animación con el banco de `wolfanim.cfg` y validan los rangos contra el número de fotogramas del modelo MD3/MDC/MDS. Todas las fuentes nuevas fueron escritas de forma independiente; **no se ha incorporado código GPL** de los proyectos de referencia.

## Archivos nuevos

- `rtcw_animscript.h`: interfaz de catálogo y estructuras estáticas.
- `rtcw_animscript.c`: lexer de caracteres sin heap, parser estructural de cuatro secciones, validador de comandos `LEGS/TORSO/BOTH` y generador TSV.
- `rtcw_animscript_main.c`: herramienta `rtcw_script` para archivos locales o `-vfs` con manifiesto multi-PK3.
- `tests/test_codec_v10.py`: pruebas sintéticas y 100 mutaciones deterministas.

## Ejemplo local

```bat
rtcw_script.exe catalog body.mds wolfanim.cfg wolfanim.script graph.tsv
```

El modelo y los dos archivos de animación deben corresponder a la **misma variante de personaje**. Genera `graph.tsv` con columnas `line`, `section`, `state`, `movement`, `event`, `old_state`, `new_state`, `condition`, `part`, `clip`, `clip_matches`, `playable_matches`, `duration_override_ms`, `sound`, `showpart_mask`, `hidepart_mask`, `validation`.

Los clips vinculados aparecen como `resolved` solo si el nombre es único en el banco de animaciones **y** su intervalo de fotogramas cabe en el modelo. Los nombres ausentes o duplicados se muestran como `unresolved_or_ambiguous`, sin cambiar nombres ni inventar clips.

## Ejemplo virtual: base + mods

Prepara `mounts.txt` en orden de menor a mayor prioridad, uno por línea:

```text
C:/RTCW/Main/pak0.pk3
C:/RTCW/Main/pak1.pk3
C:/RTCW/Mods/creatures.pk3
```

Luego:

```bat
rtcw_script.exe catalog-vfs mounts.txt models/players/undead/body.mds inspeccion_undead
```

Las rutas de modelo son **ilustrativas**: elige rutas que realmente salgan del catálogo PK3. En la misma carpeta virtual, el programa busca `wolfanim.script`, y prefiere `wolfanim.cfg` antes de `animation.cfg`. Extrae los recursos a la nueva carpeta `inspeccion_undead`, escribe `graph.tsv`, e incluye `sources.tsv` con el archivo PK3 ganador de modelo, CFG y script. Las coincidencias por directorio se etiquetan como *candidatas*, no como hechos de identidad semántica.

## Secciones reconocidas (lectura estructural)

- `animations`: `state ESTADO` -> `MOVIMIENTO` -> `CONDICIÓN` -> órdenes de animación.
- `canned_animations`: mismo esquema, mantenido en categoría distinta.
- `statechanges`: `statechange ANTERIOR NUEVO` -> `CONDICIÓN` -> órdenes.
- `events`: `EVENTO` -> `CONDICIÓN` -> órdenes.
- `defines`: se reconoce la sección, pero no se evalúa `set`, máscaras de condiciones ni macros.

Para los comandos se admiten partes `BOTH`, `LEGS` y `TORSO`, una o dos animaciones por orden, `duration N`, referencias `sound` y máscaras de `showpart/hidepart` 0..7. Se conservan expresiones de condición como **texto no interpretado**, sin ejecutarlas ni predecir qué rama elegiría el juego. Cualquier parámetro de comando desconocido se rechaza explícitamente. No se calculan prioridades efectivas ni mezcla final del esqueleto a partir del script.

## Límites deliberados

- Lectura ISO C89; sin memoria dinámica; enteros, buffers estáticos; sin dependencias externas; sin `stdin`.
- Máximo 32 768 tokens de 95 caracteres, 2048 órdenes, profundidad estructural limitada a cuatro niveles. El lexer acepta sintaxis ASCII, comillas simples solo como parte de token y dobles para cadenas delimitadas; comentarios `//`, `/* ... */`, `#`.
- El mapeo de clip por nombre funciona mejor con `wolfanim.cfg` versión 2, que incluye nombres explícitos. Los `.cfg` antiguos con filas numéricas reciben `row_NNN` y pueden quedar sin vincular al script.
- El resultado no prueba que un monstruo real funcione dentro del juego ni que el motor se comporte exactamente como RTCW; requiere corpus de juego y verificaciones adicionales.
- En la v0.10 no se interpreta el lenguaje completo de las condiciones, ni se reproducen transiciones en tiempo real, mezcla de huesos, efectos de sonido o cambios de visibilidad. Se produce un **grafo textual de referencias**, no una máquina de estados ejecutable.
- Los paquetes PK3 deben tratarse como datos no confiables; el extractor protege rutas habituales, pero no es un sandbox perfecto frente a enlaces simbólicos. Utiliza directorios nuevos y sin privilegios.

## Compilación y prueba

```sh
make clean && make CC=gcc
python3 tests/test_codec_v10.py ./rtcw_script
```

Para Win32 utiliza `BUILD_WINDOWS.bat` en MSYS2 mingw32; la compilación de Windows real está pendiente de verificación en ese entorno.

## Código original consultado

`RTCW-SP-master.zip` proporcionado por el usuario, especialmente `src/game/bg_animation.c` (`BG_AnimParseAnimScript`, `BG_ParseCommands`, `BG_ParseConditions`) y `src/cgame/cg_players.c` (ruta de `wolfanim.script`). Solo se consultaron para entender formato y semántica: código C89 nuevo bajo CC0, sin copiar estructuras ni rutinas GPL.
