# RTCW Natsuki Monster Codec v0.14 — JPEG y materiales VFS

Entrega **SOURCE ONLY**. Código C89 original bajo CC0; **no** incluye modelos, texturas comerciales, código GPL copiado, ejecutables ni motores propietarios.

## Novedades comprobables

- `rtcw_vfs_material.c/.h`: enlaza las superficies que ya está renderizando `rtcw_rig` con materiales de scripts `.shader` y busca las imágenes ganadoras entre un máximo de 16 PK3 montados. Emite mapas de resolución y procedencia por pose, con marcador `-` si el material no se encuentra. No inventa las partes de ningún monstruo.
- `rtcw_jpeg.c`: decodifica JPEG baseline secuencial, 8 bits, 1 o 3 canales (grayscale y YCbCr), Huffman y cuantización 8-bit; muestreo 4:4:4, 4:2:2, 4:2:0 comprobado, IDCT y RGB enteros. Dimensión máxima de 1024×1024, JPEG comprimido máximo de 8 MiB. Rechaza JPEG progresivos, aritméticos, CMYK y con reinicios DRI no nulos.
- `rtcw_texture.c` y `rtcw_material.c`: el visor selecciona `.tga`, `.jpg` o `.jpeg`; para rutas sin extensión prueba esas alternativas en orden. La ruta local heredada mantiene el comportamiento anterior.
- `rtcw_texrender.c`: nuevo modo de render con tabla por superficie, además de la ruta local anterior.
- `rtcw_rig_main.c`: **`run-vfs` sin `TEXTURE_ROOT` genera automáticamente** `pose_NNN_color.ppm`, hojas de contacto a color y un reporte de materiales por pose. `TEXTURE_ROOT` explícito sigue dando prioridad a la ruta local del usuario.
- `rtcw_imagecheck_main.c`: comando independiente para comprobar imágenes soportadas y convertirlas en PPM P6, sin sistema de modelos.

## Ejemplo mínimo multi-PK3

**mounts.txt** (orden de menor a mayor prioridad):

```text
C:/RTCW/Main/pak0.pk3
C:/RTCW/Main/pak1.pk3
C:/RTCW/Mods/creatures.pk3
```

**rig.txt** (sin `TEXTURE_ROOT`; referencias de piezas explícitas):

```text
ROOT_PART 0
ATTACH tag_head @models/players/example/head.md3 TORSO - PART:1
```

Comandos (el ejemplo requiere que las rutas, el tag y los scripts realmente existan):

```bat
BUILD_WINDOWS.bat
rtcw_rig.exe run-vfs mounts.txt models/players/example/body.mds scenario.txt rig.txt monster_preview
rtcw_imagecheck.exe monster_preview/vfs_tex_00.jpg verified.ppm
```

El resultado genera un informe `material_sources_000.tsv` por pose, con `surface`, `material`, `image`, `shader_definition`, `winning_pk3` y `status`. El archivo `materials_000.tsv` permite al renderer localizar cada imagen extraída en una ruta de trabajo. La procedencia de modelo/skin/accesorios se mantiene en `sources.tsv`.

Los shaders del juego no siempre corresponden a una textura única; aquí se selecciona **la primera referencia simple de `map` / `clampmap` / `animMap` compatible**, sin simular múltiples pasadas, efectos `tcMod`, iluminación ni blending de GPU. La búsqueda de archivos `.shader` se hace en orden inverso de archivos ganadores del VFS: es **heurística determinista**, no reproducción garantizada del orden global de shaders de RTCW. Tampoco se detectan todas las variantes de scripts usados por mods.

## Protocolo

C89 (`-std=c89`), sin `malloc`/`calloc`/`realloc`/`free`, sin `float`/`double`/`long long`, sin `stdin` ni bibliotecas externas de imagen. Tamaños y rutas limitados; una imagen que no se pueda decodificar no se considera válida aunque exista en un PK3. La ruta de presentación usa un rasterizador de software orientado a inspección, no renderizar de forma idéntica a id Tech 3.

**No ejecutado:** compilación MSYS2/Win32 en hardware del usuario, pruebas con monstruos comerciales, paridad exacta de shaders y materiales; sin compatibilidad JPEG progresivo, PNG, PCX ni DXT. El DEFLATE y VFS conservan sus límites de versiones anteriores.

## Pruebas sin librerías Python externas

```sh
make CC=gcc all
python3 tests/test_codec_v14.py ./rtcw_rig
python3 tests/test_codec_v14_images.py ./rtcw_imagecheck
```

Los JPEG sintéticos de las pruebas están codificados en Base64 **dentro de un archivo fuente Python**, de modo que la entrega no exige Pillow ni contiene imágenes comerciales. Para las pruebas de versiones anteriores, ejecutar `tests/test_codec.py` y `tests/test_codec_v02.py` a `test_codec_v13.py` con los argumentos indicados en cada archivo. 

**Seguridad:** usa paquetes y directorios de salida confiables; el código ZIP/VFS no ofrece mitigación completa frente a enlaces simbólicos preexistentes ni carreras del sistema de archivos. El modo `run-vfs` requiere una carpeta de salida NUEVA. No ejecutar con privilegios elevados sobre mods desconocidos.
