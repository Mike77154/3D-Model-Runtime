# RTCW Natsuki C89 Codec — v0.19: Audit por criatura

Fecha: 2026-11-02. Código original de esta ampliación distribuido bajo CC0-1.0.

## Resumen

`rtcw_audit` revisa modelos MD3/MDC/MDS decodificados por el codec existente,
extrae una muestra determinista de fotogramas, evalúa topología básica, huesos,
tags, UV, clips y, si se usa VFS, materiales y procedencia de recursos.

**No pretende detectar automáticamente todos los agujeros de una malla:**
una frontera abierta o una parte ausente puede ser un diseño deliberado. Una
superficie con UV constantes también puede ser legítima. No clasifica
criaturas por especie o nombre sin un catálogo comprobado.

## Compilación Win32

En `cmd.exe` con MSYS2 mingw32 en PATH:

```bat
BUILD_WINDOWS.bat
```

También: `make CC=gcc all` bajo Linux/MSYS2 con Make instalado.
El nuevo binario es `rtcw_audit.exe` (18 herramientas en total).

## Uso

Los directorios de salida deben **no existir** antes de ejecutar:

```bat
rtcw_audit.exe local creature.mds wolfanim.cfg - audit_individual 24
rtcw_audit.exe local creature.md3 - - audit_sin_cfg
rtcw_audit.exe vfs mounts.txt models/creature/body.mds audit_pk3 24
rtcw_audit.exe batch-vfs mounts.txt audit_coleccion 128 24
```

- `local`: modelo local, CFG o `-`, skin o `-`, directorio nuevo, máximo de fotogramas (opcional).
- `vfs`: lee el modelo desde la superposición de paquetes, intenta encontrar `wolfanim.cfg`
  o `animation.cfg` en su directorio y, de existir, `body.skin`; resuelve los
  materiales con el mismo VFS que el visor. Detecta la procedencia de cada recurso.
- `batch-vfs`: audita hasta `max_models` modelos ganadores, en carpetas
  `model_000`, `model_001`, etc. El catálogo principal `catalog.tsv` indica
  `AUDITED`, `FAILED` o `SKIPPED_PATH`. El máximo por llamada es 256.

`max_frames`: 1–256 (predeterminado 24). Se toma una muestra uniforme de la
animación y siempre se inspecciona también el último fotograma; por ello el
número realmente revisado puede superar en uno el límite solicitado. Para
asegurar inspección completa, usa un máximo igual o mayor al número de frames
si este no excede 256.

## Archivos generados

- `summary.tsv`: formato, dimensiones del modelo, número de errores, advertencias,
  notas, y cobertura `full` o `sampled`.
- `findings.tsv`: nivel, código, elemento afectado y explicación.
- `frames.tsv`: geometría por fotograma, triángulos degenerados y límites espaciales Q6.
- `surfaces.tsv`: superficies, shaders declarados y rangos de geometría.
- `uvs.tsv`: límites UV Q12, coordenadas fuera de [0,1] y superficies UV constantes.
- `clips.tsv`: datos `wolfanim.cfg`/`animation.cfg` cuando el parser los acepta.
- `sources.tsv` (VFS): modelo/CFG/skin y paquete ganador.
- `material_sources.tsv`, `materials.tsv` (VFS): resolución de materiales por etapas
  y PK3 de origen. Si falla la resolución se registra una advertencia.
- `catalog.tsv` (solo batch): todas las criaturas procesadas y su estado.

## Interpretación de resultados

- `ERROR`: prueba de integridad fallida (por ejemplo `CLIP_OUT_OF_RANGE`).
- `WARN`: indicio para revisar visualmente, no necesariamente un modelo inválido.
- `NOTE`: información o validación no realizada.

La verificación se realiza sobre lo que el decoder existente puede abrir; no
certifica equivalencia con iortcw, no simula AI y no valida los enemigos del
juego original si no se aportan sus recursos. Los clips `wolfanim.script` no
son ejecutados por el auditor.

## Restricciones de ingeniería

Ningún módulo nuevo usa malloc/calloc/realloc/free, heap, VLA, `float`,
`double` o `long long`. C89, almacenamiento estático, enteros/fixed point,
formatos de reporte tabulares y validaciones de rangos. El decodificador base
mantiene sus límites anteriores de tamaños de modelos y buffers.

## Pruebas

```sh
make CC=gcc all
./RUN_ALL_TESTS.sh
```

`tests/test_codec_v19.py` contiene casos sintéticos de MD3/MDC/MDS, modelado
degenerado, UV constantes, CFG incorrectos, multi-PK3, sobrescrituras de mods,
texturas faltantes, archivo PK3 corrupto y auditoría por lotes.

Pendientes: corpus comercial completo de RTCW, pruebas nativas Windows/MSYS2,
paridad visual con el motor original y análisis topológico de huecos no triviales.
