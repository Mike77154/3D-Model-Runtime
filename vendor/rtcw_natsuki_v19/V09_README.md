# RTCW Monster Codec v0.9 — Natsuki 🍓

## Alcance

Esta versión añade una **herramienta de catálogo y secuencias de animación** que utiliza el decodificador de modelos v0.8 (MD3/MDC/MDS) sin modificar su API ni el VFS. Archivos nuevos: `rtcw_clipbank.h/.c`, `rtcw_clips_main.c` y `tests/test_codec_v09.py`.

La observación más relevante de la auditoría de los fuentes **RTCW-SP** es que el parser `BG_AnimParseAnimConfig` de `src/game/bg_animation.c` consume más que las cuatro columnas del índice v0.7: a partir de `version 2`, después de `STARTANIMS`, cada fila tiene un **nombre**, `firstFrame`, `numFrames`, `loopFrames`, `fps`, `moveSpeed`, y opcionalmente `animBlend` y `priority`, terminando en `ENDANIMS`. El `src/cgame/cg_players.c` busca `models/players/%s/wolfanim.cfg`. La lógica de selección/acción queda en archivos `.script` separados y **no está decodificada en v0.9**.

### Comandos nuevos

```bat
rem Modelo extraído + CFG explícito (no hay que usar PK3)
rtcw_clips.exe catalog body.mds wolfanim.cfg clips.tsv

rem Clip indexado #0, hasta 24 muestras por defecto; genera OBJ + PPM + timeline.tsv
rtcw_clips.exe sequence body.mds wolfanim.cfg 0 secuencia

rem Saltar 4 fotogramas entre poses, máx. 48 vistas
rtcw_clips.exe sequence body.mds wolfanim.cfg 0 secuencia_pasos 4

rem Usar recursos ganadores de PK3 base+mods; detecta wolfanim.cfg o animation.cfg
rtcw_clips.exe catalog-vfs mounts.txt models/players/ejemplo/body.mds indice
rtcw_clips.exe sequence-vfs mounts.txt models/players/ejemplo/body.mds 0 previews
```

`mounts.txt` es el mismo de v0.8: un archivo PK3 por línea, **último gana**. `catalog-vfs` y `sequence-vfs` buscan únicamente `wolfanim.cfg` y después `animation.cfg` en la **misma carpeta virtual que el modelo** (sin asignar a ciegas clips de otras criaturas). En caso de no encontrarlos, fallan e indican que se use la variante local con ruta CFG explícita. La coincidencia por carpeta es una *candidatura*, no evidencia semántica de que el fichero corresponda a ese modelo; cada reporte VFS contiene `sources.tsv` con las dos rutas y archivos PK3 ganadores.

`sequence-vfs` deja un `model.bin`, `animation.cfg` extraídos en un **directorio nuevo** y una carpeta `sequence/` con las poses, `clips.tsv` y `timeline.tsv`. El formato de `model.bin` se detecta por firma, no por extensión.

### Especificación del catálogo

- Un máximo de **512 clips**. Campos: ID (índice sin inventar), nombre (si existe), línea origen, primer fotograma, cantidad, fotogramas en bucle, milésimas de FPS, `moveSpeed` con signo, mezcla, prioridad, fotogramas reales del modelo y estado.
- Soporte del `wolfanim.cfg` con **`version 2` y nombres explícitos**, `STARTANIMS` y `ENDANIMS`; `version 1` sin nombres y 5–7 campos; tablas Quake/`animation.cfg` sencillas de 4 campos. Cabeceras auxiliares se ignoran, nunca se inventan nombres para clips sin nombre (`row_000`, etc.).
- FPS decimal con precisión máxima de **tres decimales** representado en milli-FPS, sin `float` o `double`. Se preserva `moveSpeed = -1` (valor de cálculo automático que utiliza el juego).
- Si un rango sobrepasa la cantidad real de fotogramas, se cataloga como `outside_model_frame_range` pero **se prohíbe generar vistas**. Rechaza archivos truncados, filas inválidas y overflow de enteros.
- Poses exportadas en **OBJ** y wireframe **PPM P6** (800×800), sin texturizado para secuencias, y tiempos en milisegundos calculados con enteros. Muestreo de hasta 24 poses automático, o hasta 48 con `stride` explícito. No crea GIF/MP4 ni reproduce un clip en tiempo real.
- El parser de fotogramas *todavía no* resuelve la semántica completa de `wolfanim.script`, `HEADFRAMES`, el LOD runtime ni interpolación temporal del clip; cada pose corresponde a un fotograma discreto. `rtcw_mds_anim` sigue siendo el CLI para interpolación manual torso/piernas. El catálogo no es un validador de sincronía de eventos del juego.

### Compilación (C89 estricto)

```sh
make
python3 tests/test_codec_v09.py ./rtcw_clips
```

Windows con MSYS2 mingw32 portable:

```bat
BUILD_WINDOWS.bat
```

Compila **10 programas** y añade `rtcw_clips.exe`. Probamos GCC y Clang bajo Linux y GCC+UBSan. **Win32 real y monstruos de distribución comercial pendientes** (los ZIP `RTCW-SP-master`, `RealRTCW-iortcw-migration` y `Wwasm-main` son fuentes de motores, no los modelos del juego).

### Protocolo

El código nuevo conserva `C89`, buffers estáticos, enteros/fixed-point, sin heap ni `stdin` y sin bibliotecas externas. Distribuido como fuentes CC0; el código original de los engines auditados NO se redistribuye y sus licencias originales siguen vigentes.

**Cuidado con recursos externos:** abre solo PK3 conocidos y genera sus previews dentro de directorios nuevos de trabajo. No ejecutar como administrador sobre árboles con symlinks o paquetes desconocidos.
