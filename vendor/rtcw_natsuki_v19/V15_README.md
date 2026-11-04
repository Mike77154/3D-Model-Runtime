# RTCW Monster Codec v0.15 — materiales animados, caché y transparencia

**C89 estricto · sin heap · sin coma flotante · código fuente solamente.** Los 15 programas se construyen con `make CC=gcc` (Linux/macOS), o desde MSYS2 mingw32 usando `BUILD_WINDOWS.bat` (compilación Windows nativa todavía sin probar).

## Cambios principales

1. `rtcw_texcache.c/.h`: caché estática LRU de **4 imágenes RGBA** (máximo 1024×1024 cada una). Se cargan bajo demanda y se reutilizan durante una reproducción. Se invalida cuando cambia la imagen extraída por el VFS. Estadísticas de aciertos/fallos/reemplazos. Capacidad reservada: ~16 MiB, adicional al buffer del decodificador.
2. `rtcw_material.c/.h`: `rtcw_material_reference_time(...)` y `rtcw_material_path_time(...)`; admite `map`, `clampmap` y **primer `animMap` utilizable por material**, con frecuencia FPS entera o decimal (hasta tres decimales). Las imágenes `animMap` se definen en la **misma línea** que la frecuencia; máximo 32 imágenes. Selección en milisegundos con aritmética modular C89, sin overflow de multiplicación en `unsigned long` de 32 bits. Lector de shader cacheado por nombre de archivo; invalidación explícita en extracciones temporales.
3. `rtcw_vfs_material.c/.h`: almacena qué `.shader` se encontró para cada material durante una reproducción y reutiliza texturas ya extraídas siempre que el recurso vencedor y la ruta no cambien. Se limpian los shaders temporales al terminar el escenario. Se añade `time_ms` al informe de procedencia; las columnas antiguas se conservan.
4. `rtcw_texrender.c/.h`: dos pasadas; primero píxeles opacos (escriben profundidad), después píxeles translúcidos (blend alfa RGB y profundidad de solo lectura). Píxeles completamente transparentes se descartan. Renderiza la misma imagen actual en poses separadas sin volver a decodificarla.
5. `rtcw_rig_main.c`: usa `state.now_ms` para `animMap`, escribe `texture_cache.tsv` al final de la secuencia y libera los temporales del VFS. API antigua preservada mediante wrappers de tiempo cero.
6. `rtcw_matprobe_main.c`: **15.ª herramienta** para consultar la referencia de textura a un tiempo dado. Sintaxis:

```bat
rtcw_matprobe.exe scripts/creatures.shader synthetic/skeleton 500
rtcw_rig.exe run-vfs mounts.txt models/creature/body.mds scenario.txt rig.txt salida
```

El ejemplo requiere archivos y nombres de materiales existentes. Para hacer pruebas sin recursos del juego, utiliza los fixtures de `tests/`.

## Contrato técnico

- Sin `malloc`, `calloc`, `realloc`, `free`, `float`, `double`, `long long`, `stdin`, ni bibliotecas ZIP/JPEG externas.
- C89 con `-Wall -Wextra -Wpedantic -Werror` en GCC y Clang. Sin tipos de 64 bits requeridos.
- Caché válida para recursos **inmutables durante la ejecución**. Si una herramienta externa cambia un archivo después de cargarlo, se debe reiniciar la vista o invocar el reinicio/invalidador.
- VFS: hasta 16 montajes, prioridad por orden, y limpieza de caché al comenzar cada invocación de `rtcw_rig`.
- El `animMap` se limita a una primera etapa útil, frecuencias positivas de hasta 1000 FPS y 32 rutas en la misma línea; no evalúa `tcMod`, `rgbGen`, `alphaGen`, `blendFunc`, deformadores ni todo el lenguaje shader original.
- El alpha blending representa un **preview aproximado**: dos pasadas evitan algunos defectos de profundidad, pero no ordenan los triángulos translúcidos de atrás hacia adelante ni reproducen todos los modos de mezcla de RTCW.
- Los archivos `.mds`, `.mdc`, `.md3` reales, y la fidelidad total de los enemigos comerciales siguen **sin validación de corpus**. Win32/MSYS2 requiere una compilación real en tu equipo.

## Pruebas

```sh
make CC=gcc -j2
python3 tests/test_codec_v15.py ./rtcw_rig ./rtcw_matprobe
python3 tests/test_codec_v14.py ./rtcw_rig
python3 tests/test_codec_v14_images.py ./rtcw_imagecheck
```

El código está organizado en fuentes C89 originales con licencia CC0, y el ZIP no incluye modelos, texturas, executables ni activos comerciales.
