# Pruebas de RTCW v0.18 (2026-11-02)

| Verificación | Resultado |
|---|---|
| `make CC=gcc -j2 all` (C89, `-Wall -Wextra -Wpedantic -Werror`) | PASS: 17 ejecutables |
| `make CC=clang -j2 all` (C89, mismas opciones) | PASS: 17 ejecutables |
| `tests/test_codec_v18.py ./rtcw_rig` con GCC | PASS: 19 |
| `tests/test_codec_v18.py ./rtcw_rig` con Clang | PASS: 19 |
| Suite v0.18 con GCC y `-fsanitize=undefined -fno-sanitize-recover=undefined` | PASS: 19, sin reportes UBSan |
| Suites v0.01–v0.18 con GCC | PASS: 19 suites, 650 comprobaciones |
| Pruebas con los PK3 originales de RTCW | PENDIENTE |
| Compilación MSYS2 mingw32 real | PENDIENTE |

## Escenarios ejercitados

- Material de dos etapas, TGA base + segundo `animMap` TGA/JPEG.
- JPEG de la segunda etapa sustituida por un mod; ganancia de prioridad VFS sin distinción de mayúsculas.
- Secuencias `0`, `500`, `1000` ms: vuelve al primer fotograma después de un ciclo, y el PPM realmente cambia.
- Material de cuatro etapas, tres texturas adicionales extraídas desde un PK3 al seleccionar un shader explícito.
- Proveniencia por etapa y mapas de siete columnas válidos.
- Imagen de etapa faltante: `missing` sin adjudicación ficticia de recursos.
- Ruta `../secret.tga` en etapa secundaria: error seguro, sin escritura fuera del directorio.

El paquete usa solamente fuentes y fixtures sintéticos. No certifica fidelidad visual equivalente al renderer real de RTCW.
