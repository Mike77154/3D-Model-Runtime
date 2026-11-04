# RTCW Natsuki Monster Codec v0.18 — VFS + shader stages

## Nuevo circuito de datos

`PK3 base y mods -> VFS (última ruta montada gana) -> .shader -> stage 0..3 -> extracción por etapa -> TSV -> rasterizador Q12 -> PPM`.

El modelo MD3/MDC/MDS, la selección explícita de animaciones, el ensamblado por tags, el escaneo de skins y la exportación OBJ/PPM conservan la funcionalidad anterior. **No hay IA del juego ni GPU real**.

### Interfaz de materiales

La salida `materials_000.tsv` contiene **siete campos, sin encabezado**, una línea por superficie:

`surface_index TAB material_name TAB stage0_local_path TAB shader_local_path TAB stage1_local_path TAB stage2_local_path TAB stage3_local_path`

Para una etapa no utilizada o no encontrada se emite `-`. Las rutas a imágenes son archivos extraídos del VFS de mayor prioridad. La lectura sigue admitiendo los mapas antiguos de 3/4 columnas. El renderizador consulta `stage1..stage3` según el número de etapas declarado en `.shader`, sin tratar un recurso faltante como disponible.

`material_sources_000.tsv` añade las columnas `stage1_image`, `stage1_pk3`, `stage1_status`, ... hasta la tercera etapa adicional, conservando todas las columnas antiguas de procedencia de stage 0. Los estados son `not_used`, `found`, `missing` y `unsupported`.

**Límite:** hasta cuatro etapas por material y hasta 32 entradas `animMap` por etapa. `rtcw_stagefx` acepta `tcMod scale/scroll`, `rgbGen const/identity`, `alphaGen const/identity`, `blendFunc add/blend/filter` y `alphaFunc GT0/GE128/LT128`. No interpreta `tcMod rotate/turb`, `deformVertexes`, iluminación dinámica, técnicas de profundidad especiales ni ordenamiento exacto de capas translúcidas como la GPU original. Es un visor de inspección, no un renderer bit-a-bit de RTCW.

## Compilar

En Linux: `make CC=gcc -j2 all` o `make CC=clang -j2 all`. Se usa C89 estricto, memoria fija (sin malloc/calloc/realloc/free, sin float/double/long long), y bibliotecas estándar para IO/archivos; no exige zlib ni otros módulos externos.

En Windows (MSYS2 mingw32): ejecutar `BUILD_WINDOWS.bat`. **La compilación nativa Windows sigue sin verificarse aquí.**

## Ejecutar

Crear `mounts.txt` con una ruta de PK3 por línea, juego base primero y mods después:

```text
C:/RTCW/Main/pak0.pk3
C:/RTCW/Mods/mycreatures.pk3
```

Después, usando **rutas de modelo auténticamente presentes** en tu catálogo y los archivos de plan compatibles con el reproductor:

```bat
rtcw_vfs.exe index mounts.txt resources.tsv
rtcw_rig.exe run-vfs mounts.txt models/players/ejemplo/body.mds scenario.txt rig.txt preview
```

Un ejemplo de shader explícito en `rig.txt` (si existe en tus PK3):

```text
SHADER_FILE @scripts/characters.shader
```

En `preview` se obtienen `pose_NNN_color.ppm`, `materials_NNN.tsv`, `material_sources_NNN.tsv` y `texture_cache.tsv`. Los shaders alternan `animMap` a partir del tiempo de cada pose. El sistema no descubre automáticamente qué modelos componen una criatura: las piezas deben especificarse con tags explícitos.

## Pruebas reproducibles

```sh
python3 tests/test_codec_v18.py ./rtcw_rig
```

Son 19 comprobaciones de extremo a extremo sobre PK3 sintéticos y assets generados por los tests. Se probaron **19 suites históricas, 650 comprobaciones acumuladas**. Se compiló con GCC y Clang en Linux y se ejecutó la suite nueva con el UBSan de GCC. Los PK3 comerciales de RTCW y Win32 nativo siguen **sin verificar**.

## Seguridad y procedencia

Cada etapa se comprueba con nombres de ruta virtuales seguros; las rutas relativas que suben directorios se rechazan. Se registra el archivo PK3 ganador para cada etapa y se informa de lo que falta. Las imágenes de prueba son originales sintéticas; los assets y el código GPL original de RTCW no se incorporan al paquete. Los archivos fuente originales de este códec se distribuyen con CC0-1.0.
