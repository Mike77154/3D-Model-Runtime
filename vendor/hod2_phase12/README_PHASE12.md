# The House of the Dead 2 — Phase12 / ejecutables opcionales para datos

## Resultado

Las tablas necesarias por el lector HOD2 C89 ahora se encuentran en archivos
independientes de los ejecutables originales, uno por cada perfil:

- `tables/HOD2_1999_tables.h2tb`
- `tables/HOD2_2000_tables.h2tb`

Cada archivo contiene las secciones de datos inicializados `.rdata` y `.data`
originales, pero NO la seccion de codigo ejecutable `.text`, ni cabeceras PE,
ni recursos `.rsrc`. Los punteros virtuales de las tablas se mantienen para
que todos los algoritmos del codec original resuelvan las mismas referencias.
No se requiere el ejecutable para consumirlos con el toolkit Phase12.

## Formato H2TB v1

Binario little endian sin estructuras dependientes del compilador.

```
00  magic "H2TB"
04  u32 version = 1
08  u32 profile (0=1999, 1=2000)
0C  u32 segment_count (=2)
10  segment 0: u32 virtual_address, length, file_offset, FNV1a32
20  segment 1: u32 virtual_address, length, file_offset, FNV1a32
30  data bytes (segment 0 seguido de segmento 1)
```

La comprobacion de integridad FNV32 detecta corrupciones accidentales de las
secciones, pero no es un mecanismo criptografico.

## Compilar y usar (MSYS2 MinGW32)

Abrir terminal MinGW32 en `native_c89`:

```bat
build_win32.bat
hod2c89.exe tablescheck 1999 "..\tables\HOD2_1999_tables.h2tb"
hod2c89.exe skeleton 1999 "..\tables\HOD2_1999_tables.h2tb" 15
hod2c89.exe parts 1999 "..\tables\HOD2_1999_tables.h2tb" 15
```

Para exportar un zombie completo con abdomen articulado:

```bat
hod2c89.exe motdump 1999 "..\tables\HOD2_1999_tables.h2tb" "C:\HOD2\pol_mot_tex\mot" 15 187 "C:\HOD2\tipo15.h2mc"
hod2c89.exe skincharacter 1999 "..\tables\HOD2_1999_tables.h2tb" 15 "C:\HOD2\pol_mot_tex\pol" "C:\HOD2\tipo15.h2mc" "C:\HOD2\tipo15.glb" "C:\HOD2\pol_mot_tex\tex"
```

Los directorios POL, MOT y TEX son **recursos del juego aparte**; el .h2tb
contiene solo tablas, no geometria, texturas ni fotogramas MOT. La aplicacion
C89 debe estar compilada con el codigo Phase12: la anterior Phase11 no
reconoce automaticamente el nuevo formato.

Para generar las tablas de otra copia compatible del juego (operacion unica):

```bat
hod2c89.exe tablesexport 1999 "Hod2 - y99.exe" "HOD2_1999_tables.h2tb"
hod2c89.exe tablesexport 2000 "Hod2 - y2000.exe" "HOD2_2000_tables.h2tb"
```

Para producir archivos CSV en un directorio existente:

```bat
hod2c89.exe tablesdump 1999 "..\tables\HOD2_1999_tables.h2tb" "..\csv1999"
```

Hay cinco CSV por perfil: `skeletons`, `extra_parts`, `model_banks`,
`motion_banks`, `texture_descriptors`.
Los CSV son informes legibles; para las referencias de vertices y datos de
skinning sin perdida, usar los archivos `.h2tb` directamente.

## Validacion

- GCC: `-O2 -std=c89 -pedantic -Wall -Wextra -Werror`, aprobado.
- Clang: misma configuracion, aprobado.
- Suite nativa previa `make check`: dos ejecutables de pruebas, aprobado.
- 358 comparaciones funcionales sobre 1999 y 2000, todas coinciden.
  Incluyen 172 esqueletos, 172 listados de partes y 14 casos adicionales
  (`skinaudit`, `tail23`, `motions`, `skinmap`, `skincharacter`, `motdump`, `tex`).
- Salidas de los cinco CSV de cada version comparadas byte a byte entre
  el .exe y el .h2tb: coinciden tras descartar entradas erroneas que
  apuntan a la seccion ejecutable, no a un banco de identificadores.
- Se rechazan: perfil incorrecto, corrupcion y truncamiento del archivo.
- Exports GLB del tipo 15, version 1999 y 2000, comprobados.

**No se ha probado compilacion en Windows real**. Se validaron GCC y Clang
en el entorno de pruebas actual. No afirma perfeccion visual de todos los
personajes: siguen pendientes las rutas especiales previamente identificadas.

**Nota sobre datos**: estos .h2tb conservan secciones de datos no ejecutables
enteras (unos 869 KiB cada una) por compatibilidad, no solo tablas ya
nombradas. Una entrada anomalamente interpretada como banco de modelos 460
(`harold.bin`) remitía a `0x00450062` dentro de `.text` y es excluida de los
CSV. Esa posicion no se copia al .h2tb.
