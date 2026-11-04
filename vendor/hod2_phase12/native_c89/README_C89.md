# HOD2 Phase09: toolkit nativo C89 (port parcial)

Este directorio incorpora un **port nativo verificable** sobre el proyecto original `HOD2_Character_CODEC_Phase07_86x2_FULL`. No representa todavía una traducción completa de los 17 scripts Python ni de las 3 tareas Java de Ghidra. Los scripts originales se conservan como referencias y comparadores.

## Contrato técnico

- ISO C89: `-std=c89 -pedantic -Wall -Wextra -Werror`.
- Sin `malloc`, `calloc`, `realloc`, `free` ni buffers de longitud variable.
- Sin variables `float`/`double`, sin dependencia de bibliotecas gráficas o de terceros.
- Sin `stdin`: entradas únicamente por argumentos y archivos; `stdout` y `stderr` solo reportan.
- Lee representaciones IEEE754 originales como **bits de 32 bits**, no efectuando aritmética de punto flotante. Los valores de vértice de GLB son copiados byte a byte desde POL. Los límites `min`/`max` se escriben como decimal JSON usando aritmética decimal entera; es distinto de cambiar la representación de los vértices.
- Memoria estática grande, sin asignación dinámica. El ejecutable Linux compilado localmente midió aproximadamente **237 MiB de BSS** (más otros segmentos y reservas de proceso). En Win32 de 32 bits la disponibilidad real depende del espacio de direcciones y del enlazador; no se ha comprobado la compilación Windows aquí.
- Sin archivos EXE/BIN originales en esta entrega: las pruebas reproducibles se hicieron con **fixtures sintéticas**. No afirmar que se recorrieron nuevamente los 172 modelos reales en este port.

## Compilación

Desde MSYS2 **MinGW32** (32 bits) en la carpeta `native_c89`:

```sh
mingw32-make CC=gcc
./hod2c89.exe --help
```

En una consola Windows con `gcc` configurado también puede ejecutarse `build_win32.bat`. Desde Linux/macOS para desarrollo:

```sh
make clean all check
```

Si solo tienes `make` y el archivo de compilación de MinGW no autodetecta `mingw32-make`, utiliza `make CC=gcc`.

## Ejemplos de uso

```sh
hod2c89.exe prs compressed.bin expanded.bin
hod2c89.exe pol boss6.bin 0
hod2c89.exe glb boss6.bin 0 boss6_static.glb
hod2c89.exe glbcheck boss6_static.glb
hod2c89.exe tex 1999 "Hod2 - y99.exe" tex_bank.bin 0 0 tex000.png
hod2c89.exe glbtex 1999 "Hod2 - y99.exe" boss6.bin 0 tex_bank.bin 0 boss6_static_textured.glb
hod2c89.exe skeleton 1999 "Hod2 - y99.exe" 0
hod2c89.exe parts 1999 "Hod2 - y99.exe" 0
hod2c89.exe clips 1999 0
hod2c89.exe audit 1999 "Hod2 - y99.exe" extracted_POL audit1999.csv
hod2c89.exe motions 1999 "Hod2 - y99.exe" extracted_MOT 0 clips0.csv
hod2c89.exe motdump 1999 "Hod2 - y99.exe" extracted_MOT 0 11 sample.h2mc
hod2c89.exe peprobe 1999 "Hod2 - y99.exe" static_pe_report.txt
```

El índice del banco TEX de `glbtex` debe corresponder al POL suministrado. Los argumentos `1999` y `2000` identifican los **dos ejecutables analizados**, no garantizan un tercero específico de 2001.

## Phase08: personajes rígidos animados y lote

Ver **`PHASE08_CHANGELOG.md`** para instrucciones y resultados. Se incorporan `character`, `charactertex` y `batch`. El nuevo modo permite agrupar las mallas rígidas presentes en el árbol de huesos, incrustar imágenes TEX compatibles, y convertir un H2MC validado en canales GLB de rotación/traslación; el lote emite un CSV con estado de cada uno de los 86 tipos.

```sh
hod2c89 character 1999 "Hod2 - y99.exe" 7 extracted_POL character07.glb
hod2c89 charactertex 1999 "Hod2 - y99.exe" 7 extracted_POL extracted_TEX character07.glb type07.h2mc
hod2c89 batch 1999 "Hod2 - y99.exe" extracted_POL extracted_TEX output_dir report99.csv
make check
./test_character --keep
```

`--keep` genera GLB/PE sintéticos para inspección. El lote **no** afirma skinning completo, y las variantes POL especiales no se han incluido.

## Alcance exacto del C89 añadido

| Módulo | Funcionalidad | Grado de implementación |
|---|---|---|
| `hod2_binary.c` | PE32, desplazamientos de versiones, PRS y archivos | Funcional en pruebas sintéticas |
| `hod2_tables.c` | Bancos POL/MOT, modelo por ID, jerarquía de huesos, TEX | Funcional en pruebas sintéticas |
| `hod2_pol.c` | Materiales, strip/list, recuentos/UV | Funcional para el formato POL habitual; formato alternativo no exportado a GLB |
| `hod2_tex.c` | Decodificación TEX y PNG sin zlib externa | RGB565 / ARGB1555 / ARGB4444; paletas en comando TEX; algunos formatos no soportados |
| `hod2_mot.c` | Listado y extracción binaria sin pérdida de bloques MOT | **No** convierte clips a canales de animación glTF |
| `hod2_glb.c` | GLB 2.0 de una malla POL; Phase08 jerarquía rígida, un clip MOT, TEX compatibles y lote | **Rígido animado**, no skinning del abdomen ni clips múltiples |
| `character_draw_bone_part.c` | Puente C89 de vértices deformables y huesos con la implementación heredada | **No** sustituye la lógica especial `DrawCharacterPartSubparts` |
| `hod2_cli.c` | CLI de diagnóstico, extracción, exportación y auditoría | Funcional con rutas y bancos válidos |
| `../inherited/c89/` | Guardas de recursos, piezas extra, catálogo de clips con procedencia | Código C89 original conservado y compilado conjuntamente |

### Limitaciones importantes

**No hay todavía equivalencia 1:1 con `build_glb.py` + `build_abdomen_glb.py`**:

- `glbtex` sigue exportando una malla estática. `charactertex` ahora reconstruye un árbol de mallas rígidas, pero **no** las deformaciones ni los vínculos `JOINTS_0`/`WEIGHTS_0`/`inverseBindMatrices`.
- `motdump` conserva H2MC sin pérdida. Phase08 añade un **solo clip** al GLB con canales `animations`/sampler/`LINEAR`, pero no retarget ni despachadores automáticos de todos los enemigos.
- El lector POL/exportador no implementa aún todos los formatos alternativos, cambios dinámicos de abdomen/daño, variantes, armas ni subpartes especiales.
- `glbtex` omite formatos que requieran una paleta fuera del conjunto actualmente soportado y puede producir material sin imagen cuando TEX no esté disponible.
- `audit` revisa esqueleto y mallas/UV accesibles por referencias y falla si faltan archivos; **no** prueba costuras visibles, intercambios de daño, clips correctos, asignación íntegra de texturas o topología final de 172 GLB.
- `glbcheck` valida la estructura externa GLB (cabecera y chunks), no el esquema glTF completo ni el renderizado.
- `peprobe` sustituye únicamente una prueba estática de direcciones y bytes: la decompilación de Ghidra continúa requiriendo Ghidra/Java o una implementación de desensamblador propia.

## Pruebas realizadas en esta sesión

- `gcc` C89 con `-pedantic -Wall -Wextra -Werror`: **pasa**.
- `clang` C89 con esos mismos avisos: **pasa**.
- `gcc` con sanitizadores Address/Undefined: **pasa** sus pruebas sintéticas.
- PRS, lector de esqueleto/bancos, POL, TEX PNG, GLB, GLB TEX embebido, banco MOT y volcado H2MC: **fixtures sintéticas validadas**.
- `inherited/c89/test_guard.c` y `test_character_clips.c`: **pasan**.
- No se certificaron salidas de 86 × 2 al no venir en el ZIP los EXE y BIN originales.

## Licencia y conservación

Consultar `../HOTD2RE_LICENSE.txt` y las referencias de origen. No se incluyen recursos comerciales originales. El directorio `inherited/` y todos los scripts anteriores se conservan sin borrarlos ni sustituirlos por equivalentes incompletos.

## Phase09: auditoria de corpus BIN real sin ejecutables

El nuevo modulo `hod2_corpus.c` incorpora `corpus` y `corpusdiff` con compilacion C89 estricta.
No necesita EXE ni Python para el analisis. La version MinGW32 enumera con la API Win32, y la
version Linux con `dirent`. En ambos casos el lenguaje sigue siendo ISO C89 y no se usa heap.

```bat
hod2c89.exe corpus "C:\HOD2\hod2ye99" "audit99.csv"
hod2c89.exe corpus "C:\HOD2\hotd2" "audit00.csv"
hod2c89.exe corpusdiff "C:\HOD2\hod2ye99" "C:\HOD2\hotd2" "diff.csv"
```

`corpus`: recorre los directorios `pol`, `tex`, `mot`, `cam`, `evt` y `coli`,
descomprime PRS y valida modelos POL y directorios de desplazamiento MOT.
Los TEX sin EXE se auditan por lectura/descompresion pero **no** se interpretan ni
se comprueba la imagen completa sin la tabla de descriptores de textura.
`corpusdiff`: compara bytes de cada fichero que comparten nombre entre las dos carpetas.
Los ficheros solo presentes en una edicion se listan de forma explicita.

**Importante:** Los recuentos totales de modelos POL incluyen aliases/duplicados de archivo;
no equivalen a un numero de personajes ni a geometria unicamente utilizada. La prueba de
animaciones MOT confirma un directorio de bloques, no que los frames sean reproducibles
con el esqueleto original. El rig/skinning todavia depende de tablas PE, ausentes en
los dos ZIP de bancos. Consulte `PHASE09_REAL_CORPUS.md`.
