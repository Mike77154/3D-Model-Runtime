# hl1modelib — real-corpus fire test (C89)

## Estado

Versión acumulativa de la copia `hl1modelib.zip` suministrada: **todos los módulos originales conservados**. Las mejoras se hicieron sobre ese código, sin cambiar a C99, sin heap y sin `float`/`double` de runtime.

### Cambios que motivó el corpus

- `hlfp.h`: capacidad de expansión de mallas de **4,096 a 16,384 vértices**, triángulos expandidos de 8,192 a 32,768 (capacidad; no declaración de que todos los modelos requieran 32,768).
- `hlfp_mdl.c`: la conversión de UV GoldSrc `s/t` de `int16` a 16.16 ya no desplaza enteros negativos a la izquierda; ahora usa multiplicación exacta por 65,536.
- `hlfp_bundle.h/.c` (nuevo módulo portable): el motor provee un callback de lectura y buffers persistentes; carga modelo principal, `T.mdl`/`.dol` y los grupos de animación `01.mdl`, `02.mdl`... según las cabeceras GoldSrc; no utiliza `fopen` ni `malloc` dentro de la biblioteca.
- `hlfp.h`/`hlfp_smd.c`: capacidad de hasta **6 links de SMD por vértice** sin agotar la pila de las pruebas originales. Si un SMD contiene más de 6, retiene las 6 influencias mayores, renormaliza los pesos y registra `hlfp_smd_model.discarded_weight_links`. No es una transformación sin pérdida.
- `hlfp_smd.c`: recuperación de SMD con material omitido cuando la línea siguiente es claramente un registro de vértice completo; se utiliza `unnamed_material`.
- `demo_render_goldsrc_pose.c`: demo actualizado para leer `T.mdl` y grupos con el nuevo loader, capacidad de main de 8 MiB y mensajes sin `double`.
- `test_hlfp_corpusfix.c`: prueba sintética de regresión del material omitido, siete influencias y nombres de companions.
- `bundle_probe.c`, `smd_probe.c`, `corpus_probe_full.c`: CLIs de diagnóstico. No son partes obligatorias del runtime.

### Corpus real examinado

| Fuente | Datos | Resultados |
|---|---|---|
| `HL1.zip` | 82 `IDST` `.mdl` + 644 `.smd` | 82 MDL leídos; 644 SMD leídos |
| `resident Evil Cold Blood.zip` | 752 `IDST` `.mdl` + 1 `IDSQ` llamado `.mdl` | 752 MDL leídos; `IDSQ` es companion, no un `IDST` autónomo |
| `zombiestandart.rar` | 1,508 SMD | 1,508 SMD leídos, con compresión explícita de influencias excesivas |

Sobre **834 modelos principales MDL**, el bundle loader procesó correctamente **3,002 mallas no vacías** y el último fotograma de **7,086 secuencias**. En un segundo barrido de animación se decodificaron **27,489 muestras** de primer/medio/último fotograma y mezcla inicial/final sin errores reportados. Se adjuntaron **12 archivos de textura externos** y **1 archivo de grupo de secuencia externo**.

La malla más exigente observada tenía **6,316 vértices expandidos** y el máximo de triángulos en una malla fue **3,778**. El límite previo de 4,096 vértices impedía expandir el T-103 real.

### Validaciones con implementaciones de referencia incluidas

- SDK GoldSrc de Valve (`halflife-master.zip`, `hl1_source_sdk-master.zip`) y los archivos `engine/studio.h`: identificadores `IDST` y `IDSQ`, secuencias y datos de modelo.
- HalfLifeAssetManager: `src/hlam/formats/studiomodel/StudioModelIO.cpp`: usa `textureindex == 0` para requerir `T.mdl` y usa sufijos `01`, `02` para grupos externos.
- Xash3D-FWGS y OGS: revisión del árbol de estructuras studio y extensiones. Las capacidades avanzadas ya existentes (`studiohalflife`, `plus`, `ultra`) se conservaron, **sin afirmar aquí equivalencia total** con el renderer original.
- Absolute Zero: código de juego/mod referencia adicional, no una especificación independiente completa del formato.

### Compilación / ejecución

```sh
make clean all test_output.txt
make bundle_probe smd_probe corpus_probe_full
./bundle_probe "ruta/a/modelo.mdl"
./smd_probe "ruta/a/modelo.smd"
./demo_render_goldsrc_pose "ruta/a/modelo.mdl" vista 0 0
```

Compilación estricta verificada en Linux con `gcc -std=c89 -Wall -Wextra -Werror -pedantic -O2`. Pendiente probar en **MSYS2/MinGW32** específicamente (no había toolchain Win32 disponible en este entorno).

### Integración portable del bundle

```c
#include "hlfp_bundle.h"
/* Proporcionar hlfp_bundle_read_fn reader (lectura desde engine/VFS). */
static hlfp_mdl mdl;
static hlfp_u8 main_data[8*1024*1024];
static hlfp_u8 tex_data[8*1024*1024];
static hlfp_u8 seq_data[8*1024*1024];
hlfp_bundle_buffers buffers;
hlfp_bundle_stats stats;
hlfp_error err;
buffers.main_bytes=main_data;
buffers.main_capacity=sizeof(main_data);
buffers.texture_bytes=tex_data;
buffers.texture_capacity=sizeof(tex_data);
buffers.sequence_pool=seq_data;
buffers.sequence_capacity=sizeof(seq_data);
/* dentro de una función, con reader implementado:
   hlfp_bundle_load("models/zombie.mdl", reader, engine_user,
                    &buffers, &mdl, &stats, &err);
*/
```

**Importante:** los buffers deben permanecer válidos mientras se utilice `mdl`; el módulo no copia de manera oculta ni reserva memoria dinámica. Las capacidades son configurables.

### Límites que permanecen

- La decodificación de fotogramas probada no equivale a comparar cuadro por cuadro la **interpolación, los controllers, eventos o el blending de todas las animaciones** con GoldSrc/Xash; todavía falta esa comparación visual automatizada.
- Un SMD con más de seis influencias **pierde los pesos sobrantes** (aunque se contabilizan). No equiparar esta importación al weighted skinning completo de Source.
- Las previews C89 usan un **rasterizador software aproximado**, no todos los modos/materiales/filtros e iluminación del engine de origen.
- La regeneración binaria MDL/SMD/QC sólo cubre los subconjuntos reconocidos en las notas originales; no se afirma roundtrip byte-identical de cada modelo real.
- Los archivos del corpus, modelos y texturas **no se redistribuyen**; el paquete sólo contiene el código y previews generadas.

### Resultados reproducibles

- `artifacts/real_corpus_tests/bundle_results.txt`: 834 cargas del bundle con recuento de mallas y secuencias.
- `artifacts/real_corpus_tests/full_corpus_deep.csv`: 835 archivos `.mdl` inspeccionados, incluyendo el `IDSQ` no principal.
- `artifacts/real_corpus_tests/smd_probe_hl1.csv`: 644 SMD.
- `artifacts/real_corpus_tests/smd_probe_all.csv`: 1,508 SMD.
- `artifacts/real_corpus_tests/previews/`: zombi de Cold Blood, T-103 y Mr. X, realizados por `demo_render_goldsrc_pose` desde sus MDL originales.
