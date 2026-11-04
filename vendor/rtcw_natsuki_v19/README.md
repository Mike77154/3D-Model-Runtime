# RTCW Natsuki Monster Codec v0.18 — C89 SOURCE ONLY

**v0.18:** los shaders de hasta cuatro etapas ahora recuperan sus **texturas secundarias desde VFS multi-PK3**, con un mapa TSV de 7 columnas, procedencia por etapa, preferencia de mods, animMap temporal y caché invalidada al cambiar la imagen. Se conserva compatibilidad con mapas TSV anteriores. **17 herramientas**. Véanse [V18_README.md](V18_README.md) y [TEST_RESULTS_V18.md](TEST_RESULTS_V18.md). Los apartados siguientes son históricos.

**Novedad v0.17:** shaders de hasta cuatro etapas para texturas locales, `tcMod scale/scroll`, `rgbGen const`, `alphaGen const`, `animMap` y `rtcw_fxprobe`. **17 herramientas**. Consulta [V17_README.md](V17_README.md) y [TEST_RESULTS_V17.md](TEST_RESULTS_V17.md). Las secciones históricas siguientes conservan el contexto de versiones previas.

# RTCW Natsuki Monster Codec v0.16 — C89 SOURCE ONLY

**v0.16:** orden global aproximado de triángulos transparentes, soporte acotado de `blendFunc`/`alphaFunc`, diagnóstico de shaders y procedencia VFS ampliada. **16 herramientas**. Véase [V16_README.md](V16_README.md). Las notas más antiguas son históricas.

# RTCW Natsuki Monster Codec v0.15 — C89 SOURCE ONLY

**Nueva v0.15:** caché LRU de texturas, selección de fotogramas `animMap` dependiente del tiempo, transparencia alfa en dos pasadas, reutilización VFS, telemetría y herramienta `rtcw_matprobe`. **15 herramientas**. Consulta [V15_README.md](V15_README.md) para uso, restricciones y pruebas. Las secciones anteriores describen hitos históricos, no el estado vigente del motor.

# RTCW Natsuki Monster Codec v0.14 — C89 SOURCE ONLY

**v0.14:** resolución automática de imágenes TGA/JPEG por superficie a través de PK3 superpuestos, informe de procedencia y JPEG baseline sin heap ni punto flotante. Incluye **14 herramientas**, con `rtcw_imagecheck` para comprobar imágenes. [Instrucciones, límites y pruebas v0.14](V14_README.md).

**Importante:** los apartados históricos anteriores describen versiones antiguas; las funciones JPEG y la resolución VFS automática son nuevas en v0.14.

# RTCW Natsuki Monster Codec v0.13 — C89 SOURCE ONLY

> **v0.13:** nuevo filtrado de partes `PART:0..7` durante el playback y generación opcional de hojas de contacto texturizadas. Véase [`V13_README.md`](V13_README.md) para las instrucciones, los límites y los tests nuevos. Las secciones v0.12 que siguen se mantienen como documentación histórica.


**New:** See [V12_README.md](V12_README.md) for animated multi-part attachments, `rtcw_rig`, contact sheets and explicit multi-PK3 accessory loading. This complete archive keeps all 12 earlier tools, adding a 13th. Build under MSYS2 mingw32 with `BUILD_WINDOWS.bat` (native Win32 still unverified). Strict C89 / integer-only / static storage / no third-party libraries.

## Historical README (v0.10 and earlier)

**v0.10:** Nuevo [V10_README.md](V10_README.md): analizador independiente de `wolfanim.script` en C89, grafo de referencias `graph.tsv` con estados, eventos, movimientos, sonidos, condiciones como texto y validación de clips contra `wolfanim.cfg`. Nuevo ejecutable `rtcw_script` y pruebas de regresión. NO ejecuta la lógica del juego.

**v0.9:** Lee [V09_README.md](V09_README.md) para el catalogador real de `wolfanim.cfg`, la secuencia OBJ/PPM y la recuperación de clips a través de VFS multi-PK3.

**v0.8:** [V08_README.md](V08_README.md) documents the virtual multi-PK3 overlay (`rtcw_vfs`), model-family resolution across archives (`index-vfs` / `bundle-vfs`), active source reports, and 32 new tests.

**v0.7:** See [V07_README.md](V07_README.md) for `rtcw_family index/bundle`, skin/shader/texture auto-resolution, clip range candidates, manifest and automatic frame-0 preview. All previous tools are retained.


Implementación **original CC0-1.0** para inspeccionar modelos RTCW (`.md3`, `.mdc`, `.mds`) y visualizar mallas de criaturas, animaciones, materiales TGA y ensamblajes por tags.

**Protocolo técnico:** C89 estricto · enteros/fixed-point · sin `float`/`double` · sin `malloc/calloc/realloc/free` · sin `stdin` · buffers de capacidad fija · sin librerías de terceros. *Source-only*: no incluye ejecutables precompilados ni recursos protegidos del videojuego.

## Novedades v0.6 — Explorador de paquetes PK3 (ZIP)

**Nuevo ejecutable `rtcw_pk3`** (tres archivos: `rtcw_pk3.c`, `rtcw_pk3.h`, `rtcw_pk3_main.c`). Implementa un lector **independiente** de ZIP/PK3 de un solo volumen, entradas STORE (método 0) y DEFLATE (método 8), sin zlib ni dependencias externas. Inspecciona el directorio central sin extraer contenido y produce inventarios TSV por extensión y directorio.

```bat
rem Ver el inventario completo sin extraer datos
rtcw_pk3.exe list pak0.pk3 inventario.tsv

rem Agrupar modelo/skin/shader/textura según su directorio
rtcw_pk3.exe catalog pak0.pk3 catalogo.tsv

rem Extraer recursos reconocidos, conservando las rutas del archivo
rtcw_pk3.exe unpack pak0.pk3 assets all

rem Extraer solo modelos+skins+cfg, o solo texturas+shaders
rtcw_pk3.exe unpack pak0.pk3 models_only models
rtcw_pk3.exe unpack pak0.pk3 images_only textures

rem Extraer una entrada individual; el directorio destino debe existir
rtcw_pk3.exe extract pak0.pk3 models/players/someone/body.mds body.mds
```

La opción `all` **no extrae todo el PK3**: solamente archivos reconocidos `.md3`, `.mdc`, `.mds`, `.skin`, `.shader`, `.tga`, `.jpg`, `.jpeg`, `.pcx` y `.cfg`; los demás quedan fuera (se pueden extraer por nombre mediante `extract`). **TGA es el único formato de imagen realmente decodificado por el visor de texturas actual**; los JPG/PCX se inventarían y extraen, pero no se renderizan todavía.

Al desempaquetar, los archivos existentes se saltan para no sobrescribirlos. Si hay paquetes PK3 que deban superponerse en un orden determinado, **usa carpetas separadas y resuelve las prioridades deliberadamente**. No implementa virtual file system, resolución automática de dependencias `skin→shader→imagen`, ni clasificación semántica de enemigos; el inventario es un primer índice por directorio.

### Del PK3 a un monstruo texturizado

```bat
rtcw_pk3.exe unpack pak0.pk3 assets all
rtcw_texview.exe assets/models/players/someone/body.mds 0 assets preview.ppm report.txt assets/models/players/someone/default.skin assets/scripts/models.shader
```

Sustituye las rutas ilustrativas por las que te devuelva el inventario; el modelo, el `.skin` y el `.shader` deben existir. El renderer v0.5 interpreta solo un subconjunto del lenguaje shader y no garantiza las texturas correctas de todos los personajes comerciales. Las rutas en los `.shader` siguen siendo relativas a `asset_root`.

### Integridad y límites del PK3

- CRC32 de **cada archivo extraído**, rechazo de rutas ZIP absolutas, `..`, `\`, componentes vacíos y nombres demasiado largos. Los registros locales y centrales deben coincidir en nombre, método y banderas; en entradas sin descriptor de datos también se comprueban CRC/tamaños del encabezado local.
- Se rechazan archivos ZIP64, multi-volumen, cifrados y métodos distintos de STORE/DEFLATE. Límite de **32 MiB comprimidos y 32 MiB descomprimidos por entrada**, directorio de hasta 25 000 registros, longitud máxima de ruta ZIP de 240 bytes. Capacidad estática de aproximadamente 64 MiB para buffers de descompresión, sin memoria heap.
- **Solo extraer en un directorio de trabajo vacío y confiable.** El sistema de archivos puede contener enlaces simbólicos creados previamente, contra los que este extractor portátil no ofrece protección completa. No ejecutar como administrador/root con paquetes de origen dudoso.
- PK3 soporta ZIP estándar clásico; el scanner usa posiciones de archivo representables por `long` de C89, por lo que no acepta archivos de más de ~2 GiB en Win32 de 32 bits.

### Validación específica v0.6

```sh
python3 tests/test_codec_v06.py ./rtcw_pk3 ./rtcw_view ./rtcw_texview
python3 tests/test_codec_v07.py ./rtcw_family
python3 tests/test_codec_v08.py ./rtcw_vfs ./rtcw_family
python3 tests/test_codec_v09.py ./rtcw_clips
python3 tests/test_codec_v10.py ./rtcw_script
```

19 pruebas funcionales (incluida la ruta PK3 → modelo → shader → TGA → PPM), 100 mutaciones aleatorias del ZIP y ejecución instrumentada con UBSan. El lector pudo inventariar además los tres ZIP aportados como referencia de motores (no contienen los modelos comerciales buscados). **La compilación real con MSYS2 Win32 y los PK3 de distribución del juego original permanecen sin verificar.**

## Novedades v0.5

1. `rtcw_texture.c/.h`: lector TGA de 24/32 bits, tipos 2 y 10 (RLE), y TGA de grises de 8 bits, tipos 3 y 11. Maneja orientaciones inferior/superior, derecha/izquierda, alpha test y entradas truncadas. Dimensiones máximas: 1024×1024.
2. `rtcw_material.c/.h`: busca la primera textura en un bloque de shader Quake/RTCW, según `map`, `clampmap` o la primera textura de `animMap`; también resuelve nombres de shader usados directamente como rutas TGA. Admite un archivo `.shader` externo y evita rutas relativas con `..` en el nombre del material.
3. `rtcw_texrender.c/.h`: **vista texturizada real en PPM (640×640)** con triángulos rellenos, UV, interpolación afín, z-buffer, prueba de alpha y patrón magenta en materiales no encontrados. Vista isométrica simplificada; no reproduce el pipeline completo GPU de RTCW.
4. `rtcw_texview_main.c`: nuevo ejecutable `rtcw_texview` que lee modelo y opcional `.skin` y `.shader`, y genera PPM texturizado e informe.
5. `rtcw_attach.c`: nueva API `rtcw_attach_begin` / `rtcw_attach_add` para añadir **varias piezas** y conectar nuevos objetos a tags de piezas añadidas previamente (`child_tag_weapon`, `child2_tag...`). Conserva `rtcw_attach_pair` de las versiones anteriores.
6. `rtcw_multiattach_main.c`: plan de ensamblaje multiarchivo que produce OBJ, PPM alámbrico, reporte y, opcionalmente, PPM texturizado de la escena completa.

Todos los módulos v0.1–v0.4 siguen disponibles, incluida la animación esquelética MDS con torso/piernas independientes, mezcla previa al skinning y giro externo yaw de torso.

## Compilación

Linux / GCC:

```sh
make
```

Windows / MSYS2 mingw32 portable (desde carpeta que contiene los fuentes):

```bat
BUILD_WINDOWS.bat
```

En esta seccion historica se compilaban **11 ejecutables**: `rtcw_view`, `rtcw_anim`, `rtcw_mds_anim`, `rtcw_attach`, `rtcw_texview`, `rtcw_multiattach`, `rtcw_pk3`, `rtcw_family`, `rtcw_vfs`, `rtcw_clips`, `rtcw_script`.

Se verificó **GCC/Linux**, no un ejecutable Win32 real.

## Ejemplos de uso

Los modelos y texturas deben provenir de una instalación que puedas utilizar legalmente; el visor de modelos **no recibe PK3 directamente como archivo de modelo**; para ello utiliza el nuevo `rtcw_pk3` para extraer los recursos. Extrae archivos y conserva sus subrutas, por ejemplo `assets/textures/creatures/...`.

```bat
rem Modelo texturizado; assets es la raíz de imágenes extraídas
rtcw_texview.exe monster.mds 0 assets monster_color.ppm monster_report.txt

rem Seleccionar un skin externo y una tabla de shader de texto
rtcw_texview.exe monster.mds 0 assets monster_color.ppm report.txt monster.skin scripts/monsters.shader

rem Montar cuerpo + cabeza + arma + accesorios (ver ejemplo_plan.txt)
rtcw_multiattach.exe body.mds 0 - example_plan.txt monster.obj monster_wire.ppm monster_report.txt

rem El mismo ensamblaje, pero también una imagen texturizada
rtcw_multiattach.exe body.mds 0 - example_plan.txt monster.obj monster_wire.ppm monster_report.txt assets monster_color.ppm scripts/monsters.shader

rem La animación esquelética MDS original sigue disponible
rtcw_mds_anim.exe monster.mds 10 11 2048 20 21 1024 0 mixed.obj mixed.ppm mixed.txt
```

El campo de skin acepta `-` para no utilizarlo; el último campo shader opcional también acepta `-`. Los paths de materiales se tratan como relativos a `asset_root`; **los modelos, `.skin` y `.shader` se pasan como paths ordinarios**.

### Formato de plan de ensamblaje

Cada línea tiene `tag_en_la_escena ruta_del_modelo_hijo frame skin|-`. No admite espacios dentro de rutas; máximo 30 piezas por escena, más las capacidades fijas de geometría. Las líneas que comienzan por `#` o `//` se omiten.

```text
# ejemplo conceptual, modifica archivos y tags de acuerdo con tu corpus
 tag_head head.md3 0 -
 tag_weapon weapon.md3 0 -
 child_tag_hat helmet.md3 0 -
```

La segunda y siguientes piezas se prefijan con `child2_`, `child3_`, etc.; la primera utiliza `child_` para mantener compatibilidad con v0.3–0.4. Un tag de cabeza aportado por la primera pieza se puede referenciar en el siguiente paso como `child_tag_hat`.

## API destacada

```c
#include "rtcw_codec.h"
#include "rtcw_texrender.h"

if (rtcw_open("model.mds") && rtcw_decode(0)) {
    rtcw_render_textured("preview.ppm", "assets", "scripts/model.shader");
}

if (rtcw_attach_begin("body.mds", 0UL, NULL)) {
    rtcw_attach_add("tag_head", "head.md3", 0UL, NULL);
    rtcw_attach_add("tag_weapon", "weapon.md3", 0UL, NULL);
    rtcw_save_obj("combined.obj");
}
```

## Compatibilidad y limitaciones

- Los formatos `.md3`, `.mdc`, `.mds` son lectores propios, con pruebas sintéticas; **todavía no certificados contra todos los monstruos comerciales**.
- Previsualización de materiales estáticos **TGA**: no JPEG, PCX, DDS, KTX, vídeo ni compresión externa. `map`, `clampmap`, `animMap` son un *subconjunto* del lenguaje shader de RTCW, no su reproducción completa (sin efectos, multi-pass, blendFunc, tcMod, RGBGen, deformVertexes ni animación temporal de texturas).
- `rtcw_texview` visualiza fotogramas discretos; la interpolación sigue en `rtcw_anim` y `rtcw_mds_anim`, aún no conectada a una secuencia de imágenes texturizadas.
- Vista ortográfica isométrica, UV afines y alpha test fijo al 50 %. Materiales no encontrados aparecen como damero magenta (en vez de abortar).
- Las coordenadas de la vista texturizada se acotan a ±1 000 000 Q6; UV a ±65 536 Q12 para evitar desbordamientos en Win32. Los triángulos fuera de dichos rangos pueden omitirse en la vista, pero el modelo original no se altera.
- Se pueden indexar y extraer los `.shader` de un PK3 mediante `rtcw_pk3`, pero el resolutor limitado de v0.7 ahora puede operar sobre el VFS de v0.8; no cubre todos los shaders RTCW. No hay visor interactivo en tiempo real, iluminación, ni exportación PNG/JPEG.
- Máximos heredados: archivo individual 32 MiB, 32 superficies, 192 000 vértices, 262 144 triángulos y 128 tags/huesos.
- Para textura `.tga` se exige ruta relativa normalizada, sin `../`. En sistemas sensibles a mayúsculas/minúsculas se debe respetar la ruta y extensión exactas.

## Validación

```sh
python3 tests/test_codec.py ./rtcw_view
python3 tests/test_codec_v02.py ./rtcw_view
python3 tests/test_codec_v03.py .
python3 tests/test_codec_v04.py .
python3 tests/test_codec_v05.py .
python3 tests/test_codec_v06.py ./rtcw_pk3 ./rtcw_view ./rtcw_texview
python3 tests/test_codec_v07.py ./rtcw_family
python3 tests/test_codec_v08.py ./rtcw_vfs ./rtcw_family
python3 tests/test_codec_v09.py ./rtcw_clips
python3 tests/test_codec_v10.py ./rtcw_script
```

Se repitieron las cinco suites con el detector UndefinedBehaviorSanitizer. Ver `TEST_RESULTS.md` para los resultados y `SOURCE_AUDIT.md` para la procedencia de las ideas contrastadas con las tres fuentes de motor aportadas.

## Licencias

Este código de códec/visor creado desde cero se entrega como **CC0-1.0**. Las fuentes de RTCW-SP, RealRTCW y Wwasm se usaron como referencias técnicas y no se incorporan al ZIP. Sus respectivas licencias originales siguen vigentes.

## v0.11 — Explicit Animation Preview

New offline deterministic state/clip scenario runner `rtcw_preview`: MDS independent torso/legs clocks, routed movement/events/state changes, fixed-point frame sampling, loop-tail handling, VFS multi-PK3 loading, timeline and action reports, OBJ/PPM snapshots. **Conditions selected literally by the operator, not evaluated by a gameplay AI.** See [V11_README.md](V11_README.md). Build from `Makefile` or `BUILD_WINDOWS.bat`; Linux GCC/Clang tests pass, Windows native verification and real commercial models remain pending.

```bat
rtcw_preview.exe run body.mds wolfanim.cfg wolfanim.script example_preview_plan.txt my_previews
rtcw_preview.exe run-vfs mounts.txt models/players/example/body.mds example_preview_plan.txt my_mod_previews
```

Test: `python3 tests/test_codec_v11.py ./rtcw_preview`

---
## v0.19 — Auditoría de criaturas y modo por lotes

Esta entrega añade `rtcw_audit` (18.ª herramienta). Consulta [V19_README.md](V19_README.md).

```text
rtcw_audit local MODEL CFG|- SKIN|- NEW_OUTDIR [1..256]
rtcw_audit vfs mounts.txt models/creature/body.mds NEW_OUTDIR [1..256]
rtcw_audit batch-vfs mounts.txt NEW_OUTDIR [1..256] [1..256]
```

Informes: `summary.tsv`, `findings.tsv`, `frames.tsv`, `surfaces.tsv`,
`uvs.tsv`, `clips.tsv`, materiales y procedencia VFS, y `catalog.tsv` en lotes.
`RUN_ALL_TESTS.sh` ejecuta las 20 suites disponibles con fixtures sintéticos.
No se incluyen modelos comerciales ni se presupone su compatibilidad total.
