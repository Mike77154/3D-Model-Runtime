# Gun Survivor C89 v19 — integración en ModelRuntime75

**Integración independiente:** el runtime usa `src/providers/provider_gunsurvivor89.c` y registra `MODELRT_CODEC_GUNSURVIVOR (102)`; no reemplaza ni modifica el códec RE3 V028 ni IQM.

## Qué se incorporó

- `.NAB` de PC: cabecera, descriptores 0x160, todos los planos de textura, grupos de dibujo 0x18/0x20, vértices/UV/normales y jerarquía de coordenadas nativas 0x108. Renderiza con la transformación en reposo de v19, con cálculo Q8/Q12 y ejes Y-up, sin escribir archivos OBJ intermedios.
- `.NAC` acompañante y también archivo autónomo (solo catálogo): detección automática al abrir un `.NAB` del mismo nombre (ambas variantes TC/no-TC), validación de catálogo, recuento de claves por clip. Las animaciones se publican con `MODELRT_ANIM_UNBOUND`, sin declarar FPS/ticks, y NO se reproducen: `modelrt_visit(..., clip>=0, ...)` devuelve `MODELRT_ERR_UNSUPPORTED`.
- `.HMD` tipo 0x50: malla especial del Tyrant, 0x0D/0x15, polígonos triangulados, normales PSX y UV por TPage. Requiere `.tim` externo (mismo nombre o nombre sin la B final, para TAIRANTB), compatible con distinciones de mayúsculas/minúsculas en los ejemplos. **No se une automáticamente con el torso NAB; no se afirma validación de la cintura.**
- Texturas NAB (A1R5G5B5) y HMD + TIM (RGB555): decodificación RGBA8 bajo demanda con `modelrt_texture_decode_rgba`, respetando el bit A para NAB y conservando TIM opaco, sin invertir R/B. La V del modelo se invierte para coordenadas habituales del renderer; el almacenamiento de píxeles queda en orden de la fuente.
- El código no usa `malloc`, `calloc`, `realloc`, `free` ni tipos de punto flotante. Requiere arena de memoria suministrada por el llamante, igual que los demás proveedores del runtime.

## Archivos del cambio

```
include/model_runtime89.h                    CODEC_GUNSURVIVOR = 102
src/modelrt_internal.h                      declaración del proveedor
src/model_runtime89.c                       registro automático
src/providers/provider_gunsurvivor89.c      puente y decodificador nativo
Makefile                                     proveedor + pruebas sintéticas
tests/gunsurvivor_runtime_regression.c      pruebas públicas sin modelos incluidos
tests/gen_gunsurvivor_fixture.c             archivo NAB/NAC mínimo sintetizado
vendor/gunsurvivor_c89_v19/reference_tools/ código original v19 standalone
```

El directorio `reference_tools` incluye programas autónomos con `main()` para auditoría (gsnab, gsnacdump, gshmd, gstim, gsnac_verify), **no** se enlaza al runtime. El código enlazado es el proveedor C89.

## Compilar y comprobar

```
make -j4
make gunsurvivor-synthetic
make re3-regression
make native-iqm-interpolation-smoke
```

Uso con archivos propios en tiempo de ejecución (NO proporcionados en el ZIP):

```
make gunsurvivor-regression-build
./tests/gunsurvivor_runtime_regression RUTA/ENEM13.NAB 719 1 23
./tests/gunsurvivor_runtime_regression RUTA/TAIRANT.HMD 300 1 0
```

Al abrir un NAB, coloca el NAC del mismo nombre junto a él para consultar los clips. Al abrir un HMD, coloca el TIM correspondiente en la misma carpeta. La forma de comprobar es `modelrt_open_file`, `modelrt_get_info`, `modelrt_visit(...,MODELRT_ANIM_BIND_POSE, ...)`, `modelrt_texture_info_get` y `modelrt_texture_decode_rgba`.

## Validación de la integración

- Compilación completa GCC C89/pedantic: PASS (Linux).
- Nuevo proveedor aislado GCC `-std=c89 -pedantic -Wall -Wextra -Werror`: PASS.
- Corpus temporal de los 31 NAB con NAC acompañantes y 31 NAC abiertos independientemente: 62/62 PASS (conteos de triángulos, materiales, UV en rango, decode de imágenes, 703 clips marcados UNBOUND).
- Casos HMD temporales TAIRANT y TAIRANTB: 2/2 PASS; cada malla contiene 300 triángulos. TIM externo de 16-bit PSX.
- RE3 V028 `make re3-regression`: PASS.
- IQM `make native-iqm-interpolation-smoke`: PASS.
- Prueba autosuficiente NAB/NAC sintético sin recursos comerciales: PASS.

## Límites

- Los eventos, el orden temporal, interpolación y pose dinámica NAC siguen sin estar demostrados por el códec v19, y no se han implementado en la reproducción del runtime.
- El ensamblado exacto HMD-NAB para la parte inferior de Mr. X está pendiente de validar contra la ejecución original.
- No hay una verificación visual de todos los fotogramas ni pruebas físicas de modelo/skeletal animation, solamente de geometría en reposo y catálogo.
- No se realizó cross-compilation Windows i686 en este entorno (solo GCC Linux; el código es C89 y no depende de POSIX en su capa de formato).
- Si la arena es demasiado pequeña para un archivo NAB/NAC, el runtime puede omitir el catálogo NAC; el NAB sí abre cuando dispone de memoria para su propia geometría. Se recomienda una arena >=16 MB para los ejemplos suministrados.

**Entrega source-only:** los archivos de juego `.NAB`, `.NAC`, `.HMD`, `.TIM`, `.OBJ`, `.GLB` y sus imágenes NO se adjuntan en este paquete nuevo.
