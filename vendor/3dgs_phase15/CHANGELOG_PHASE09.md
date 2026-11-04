# Fase 09 — Two-Clock Crossfade / All Vendors

## Implementación

- 11 nuevos archivos C89 en `src/blend` y `src/vendors/`.
- API `GS3D_Crossfade` y `GS3D_BlendSample` con relojes independientes y tiempo de transición explícito.
- A4/A5/A6/A7/A8: entradas independientes, *routing* a lectores comunes por firma real MDL.
- MDL3/4/5: muestras morph de ambos clips, mezcla de geometría final (sin huesos).
- MDL7: claves TRS locales de ambos clips; interpolación local Q16.16 / NLERP; reconstrucción jerárquica; blend de *frame overrides* en espacio de reposo; una sola evaluación de pesos.
- APIs de transición completa y de grupo, `gs3d_crossfade_apply_all` opt-in.
- Reutilización de la fracción Q16 ya presente en el controlador de clips (sin conversión flotante).
- CLI `gs3d_blend` exporta un crossfade como secuencia OBJ/MTL.
- `SOURCES_PHASE09_DELTA.txt` lista módulos para integración en un motor existente.

## Pruebas

- `tests/test_phase09.c`: extremos exactos, 50 %, idempotencia, conflictos de vendor, clips incompatibles, control de tiempo y dos grupos.
- `tests/test_phase09_robust.c`: 2500 muestreos de tiempo deterministas sobre morph/esqueleto, re-evaluación y límites `ULONG_MAX`.
- Compilación GCC -std=c89 -pedantic -Wall -Wextra -Werror.
- Clang + AddressSanitizer + UBSan: pruebas específicas Phase09.
- Builds `SOURCES_LEGACY_ONLY` y `SOURCES_MDL7_ONLY` con flags de familia: pasan.
- Regresiones individuales de las fases 06,07,08, pose, materiales y pesos: pasan.
- Exportación probada: 5 OBJ sintéticos entre 0 % y 100 %.
- MinGW32 / Win32 todavía no comprobados (toolchain Win32 no disponible en este entorno).

## Limitaciones

- No hay duración/fps de clip inferidos automáticamente; se suministran ticks explícitos.
- No es un árbol de blend arbitrario, ni mezcla aditiva ni máscaras de huesos.
- No existe todavía *root motion* desacoplado, eventos de animación o sincronización de pasos.
- Falta verificar modelos MDL7 comerciales animados con deformadores reales; se usan fixtures sintéticos para pruebas de poses.
- El modelo activo es un singleton global, así que la API no es reentrante ni segura para acceso simultáneo.
- Si un archivo se corrompe durante la evaluación, se devuelve error, pero no se garantiza restauración transaccional de malla al fallar a mitad de la evaluación.
- El formato de pesos MDL7 depende de la capa experimental previa, no ha sido certificado sobre ejemplos comerciales completos.
