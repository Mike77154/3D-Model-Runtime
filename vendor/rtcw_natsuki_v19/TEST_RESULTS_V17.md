# RTCW v0.17 — resultados reproducibles (2026-11-02)

## Compilación

- Linux GCC, `make clean && make CC=gcc -j2 all`, `-std=c89 -O2 -Wall -Wextra -Wpedantic -Werror`: **17 herramientas PASS**.
- Linux Clang 17, `make clean && make CC=clang -j2 all` con los mismos indicadores: **17 herramientas PASS**.
- Suite v0.17 con Clang, fixture compilado en Clang: **34 PASS**.
- Suite v0.17 con GCC UBSan (`-fsanitize=undefined -fno-sanitize-recover=undefined`) en el fixture: **34 PASS**, sin avisos del sanitizador.

## Regresiones sintéticas con GCC

Se ejecutaron las 18 suites `tests/test_codec.py`, `tests/test_codec_v02.py`–`test_codec_v17.py` (incluyendo `test_codec_v14_images.py`). Resultados de comprobaciones `PASS` por suite:

| Suite | PASS | Suite | PASS |
|---|---:|---|---:|
| inicial | 7 | v0.02 | 5 |
| v0.03 | 9 | v0.04 | 13 |
| v0.05 | 15 | v0.06 | 19 |
| v0.07 | 21 | v0.08 | 32 |
| v0.09 | 39 | v0.10 | 139 |
| v0.11 | 74 | v0.12 | 34 |
| v0.13 | 24 | v0.14 | 17 |
| v0.14 imágenes | 86 | v0.15 | 35 |
| v0.16 | 28 | **v0.17** | **34** |

**Total: 631 comprobaciones PASS**, todas sobre fixtures sintéticos. La suma corrige una cifra preliminar incorrecta mencionada durante el trabajo.

## Qué verificó v0.17 específicamente

- Modulación `rgbGen const` y `alphaGen const` cambia píxeles reales del PPM.
- `tcMod scale` y `tcMod scroll` cambian el muestreo UV (fotogramas temporales a 0 y 1000 ms).
- Segunda etapa local con `blendFunc add` modifica el PPM y no se marca como parcialmente simulada.
- Rechazo de rutas peligrosas, componentes de color fuera de rango y shaders truncados.
- `animMap` elige el fotograma correspondiente según tiempo.
- Se conservan transparencias y blendFunc/alphaFunc de v0.16.

## Límites no probados

- No se han usado modelos originales comerciales de monstruos ni su colección completa de materiales.
- No hay prueba de compilación nativa Win32/MSYS2 mingw32 en este entorno.
- No existe equivalencia visual completa con el renderizador GPU del juego; efectos ajenos al subconjunto documentado quedan marcados como parciales o explícitamente rechazados.
