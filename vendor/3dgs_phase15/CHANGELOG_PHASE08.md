# Fase 08 — 3DGS C89: clips y reproducción temporal

## Implementado

- API de clips explícitos (`GS3D_Clip`) válida para **A4–A8**.
- Rangos por índices, nombres exactos y prefijo contiguo; no se infiere un clip a partir de nombres ambiguos.
- Modos: `CLAMP` (una pasada), `LOOP` (incluye transición último→primero) y `PINGPONG`.
- Interpolación Q16.16 de instantes con división de residuo segura en enteros de 32 bits; no se necesita `long long`.
- Ruteo por generación en archivos independientes `src/vendors/a4..a8/*_clip.c`.
- Distinción entre geometría interpolada y evaluación esquelética MDL7; las claves de huesos se evalúan como en Fase 07.
- `GS3D_ClipPlayer` para pausa, avance determinista y seek; controla tiempo en ticks suministrados por el llamador.
- `gs3d_clip_apply_all`: playback sincronizado de grupos de forma **opt-in**; requiere que todos los grupos animados tengan los índices seleccionados.
- CLI `gs3d_clip list` y `gs3d_clip export` para volcar secuencias OBJ/MTL.
- Tests unitarios y 8,000 vectores aleatorios de la línea temporal; Clang con ASan/UBSan en tests nuevos.

## Restricciones conservadas

C89 (`-std=c89 -pedantic -Wall -Wextra -Werror`), sin `malloc`, `calloc`, `realloc`, `free`, sin heap, sin `float`, sin `double`, sin `long long` en los módulos nuevos. Memoria estática y tamaños configurables.

## Límites

1. **MDL7 no contiene una tasa universal de fotogramas por segundo declarada en los registros inspeccionados**: `ticks_per_frame` la aporta el llamador. No inferimos FPS de los nombres.
2. El código no deduce cortes de animación a partir de nombres cuando no hay un rango explícito; la búsqueda por prefijo solo acepta un tramo contiguo.
3. MDL7 con huesos y pesos reales requiere validación adicional con corpus comercial animado; las animaciones esqueléticas siguen verificadas con fixtures sintéticos.
4. El modo `all` presupone que los grupos comparten numeración temporal; no se ejecuta de manera automática.
5. No hay mezcla de varios clips/crossfade. Tampoco se editó el Model Runtime principal.
6. En esta sesión se probó GCC/Clang en Linux y Clang ASan/UBSan de Fase 08. No se verificó Win32 MinGW32 real.

## Archivos agregados

- `src/clip/gs3d_clip_define.c`
- `src/clip/gs3d_clip_catalog.c`
- `src/clip/gs3d_clip_timeline.c`
- `src/clip/gs3d_clip_sampler.c`
- `src/clip/gs3d_clip_player.c`
- `src/vendors/gs3d_vendor_clip_router.c`
- `src/vendors/a4..a8/gs3d_aX_clip.c`
- `src/tools/gs3d_clip_main.c`
- `tests/test_phase08.c` y `tests/test_phase08_robust.c`

## Compilación rápida

```sh
sh scripts/check_phase08.sh
```

## Exportar muestras

```sh
./gs3d_clip tests/fixtures/mdl7_skeletal_sequence.mdl list
./gs3d_clip tests/fixtures/mdl7_skeletal_sequence.mdl export /tmp/demo_pose 7 0 0 2 100 25 5 0 0
```

Los valores `100` (ticks por fotograma) y `25` (paso) son la escala de tiempo elegida para la demostración, no valores extraídos del MDL.
