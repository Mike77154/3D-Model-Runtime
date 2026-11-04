# RTCW Natsuki Monster Codec v0.17 — etapas de shader (C89)

## Entrega

Fuentes solamente, CC0-1.0, sin archivos comerciales del juego. Se preservan los lectores MD3/MDC/MDS, los lectores PK3, VFS, rig, animación y renderizador del paquete v0.16. Se añaden `rtcw_stagefx.h/.c` y `rtcw_stagefx_main.c` (`rtcw_fxprobe`).

## Operaciones de shaders implementadas (subconjunto de previsualización)

- Un máximo de **4 etapas** por material con texturas locales accesibles (TGA/JPEG baseline). No hay asignación dinámica.
- `map`, `clampMap`, `animMap` lineal por frecuencia (hasta 32 imágenes de animMap).
- `tcMod scroll`, `tcMod scale`: coordenadas Q12 e incremento temporal en milisegundos (valores decimales con un máximo de 3 decimales, magnitud máxima 16); composición ordenada de escala/desplazamiento.
- `rgbGen const ( r g b )`, `alphaGen const a`, `rgbGen identity`, `alphaGen identity`. Constantes en rango [0,1], convertidas a 8 bits sin `float`.
- `blendFunc blend`, `add`, `filter`, sus pares GL equivalentes y `alphaFunc GT0/GE128/LT128`.
- Etapas locales posteriores a la primera aplicadas a triángulos visibles en una pasada ordenada por profundidad, respetando el Z-buffer opaco de v0.16. Esta composición es **aproximada**, no un render exacto por-píxel de id Tech 3.
- Las operaciones desconocidas/descartadas incrementan el contador `unhandled`. `tcMod rotate`, `turb`, `stretch`, `transform` se identifican pero **NO** se ejecutan. Otros efectos (lighting, waves, entity, environment, GPU blending arbitrario, deformVertexes, tcGen, vertex color, depthFunc) tampoco.
- El parser rechaza rutas peligrosas, límites sobrepasados y archivos malformados; un shader con más de cuatro etapas se rechaza explícitamente en lugar de dibujarse incompleto.

### Alcance de rutas VFS

La primera textura sigue resolviéndose mediante el mapa de materiales de hasta cuatro columnas producido por el VFS. Las texturas de etapas adicionales se buscan **solo bajo `TEXTURE_ROOT` local**. En modo VFS sin raíz local se contabilizan como parciales, sin adivinar recursos. La ampliación PK3 de varias etapas queda pendiente.

### Restricciones

C89 estricto, buffers estáticos, sin `malloc`, `free`, `float`, `double`, `long long`, acceso externo a bibliotecas o API gráfico. Todos los cálculos de efectos nuevos se hacen con enteros Q12 y colores de 8 bits. El renderer general conserva sus límites de mallas y texturas (ver `rtcw_codec.h`, `rtcw_texture.h`).

## Compilación

En Linux:

```sh
make CC=gcc -j2 all
# O
make CC=clang -j2 all
```

En MSYS2 mingw32 / Windows:

```bat
BUILD_WINDOWS.bat
```

Win32 no se ha compilado nativamente en el entorno de pruebas; es una instrucción para el equipo destino.

## Diagnóstico de materiales

```bat
rtcw_fxprobe.exe scripts/monsters.shader models/creature/skin 500
```

Genera una tabla TSV por etapa con ruta de imagen, blend, alphaFunc, escala Q12, scroll Q12 por segundo, multiplicadores RGBA y operaciones no interpretadas. Si no se indica `time_ms`, usa cero.

## Ejemplo de shader sintético

```text
models/test/monster
{
    {
        map models/test/body.tga
        rgbGen const ( 1 0.75 0.75 )
        tcMod scroll 0.125 0
    }
    {
        map models/test/glow.tga
        blendFunc add
        alphaGen const 0.5
    }
}
```

Ejemplo de vista con los recursos locales extraídos:

```bat
rtcw_texview.exe monster.mds 0 assets preview.ppm report.txt - scripts/monsters.shader
```

Las rutas `monster.mds` y `models/test/...` son ilustrativas.

## Pruebas

```sh
python3 tests/test_codec_v17.py gcc
python3 tests/test_codec_v16.py ./rtcw_shaderprobe
V17_UBSAN=1 python3 tests/test_codec_v17.py gcc
```

La suite nueva comprueba que `rgbGen`, `alphaGen`, `tcMod scale`, `tcMod scroll` y una segunda etapa **alteren los píxeles reales del PPM**; también comprueba texturas animadas y rechazo de entradas inválidas. Consulta `TEST_RESULTS_V17.md`.

**No verificado:** modelos comerciales de monstruos RTCW, shaders comerciales complejos, compatibilidad visual exacta con GPU RTCW, ejecución Win32/MSYS2 real. El origen de la gramática y comportamiento contrastado es el código de renderer incluido por el usuario en `RTCW-SP-master.zip`, en particular `src/renderer/tr_shader.c`. No se ha copiado el código GPL al módulo CC0.
