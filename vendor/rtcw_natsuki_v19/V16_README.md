# RTCW Natsuki Monster Codec v0.16 — Orden de transparencias y shaders

**Entrega:** código fuente CC0-1.0, C89 estricto, sin malloc/calloc/realloc/free, heap, float/double, long long ni bibliotecas gráficas externas. Los modelos originales del juego NO están incluidos.

## Qué cambia respecto de v0.15

1. `rtcw_transorder.[ch]`: cola global de triángulos translúcidos con ordenación determinista **por centroide de profundidad aproximado**, de atrás hacia delante. La primera pasada dibuja píxeles opacos y rellena el Z-buffer; la segunda mezcla transparencias comprobando ese Z-buffer sin modificarlo. El orden de las superficies de origen deja de controlar el orden de mezcla.
2. `rtcw_shaderstyle.[ch]`: parser de la **primera etapa** de una definición `.shader` con comentarios, `blendFunc blend/add/filter`, equivalentes `GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA`, `GL_ONE GL_ONE` y `GL_DST_COLOR GL_ZERO`, y `alphaFunc GT0/GE128/LT128`. Los modos no soportados y las etapas secundarias se cuentan como operaciones no interpretadas.
3. `rtcw_texrender.c`: integración del parser y el ordenamiento. API `rtcw_render_partial_shaders()` para consultar operaciones no representadas por el visor.
4. `rtcw_vfs_material.c`: el mapa de materiales acepta una cuarta columna opcional con la ruta de la definición `.shader` extraída; así el visor puede reconocer los ajustes de mezcla de recursos procedentes de PK3. Se siguen aceptando mapas antiguos de **tres columnas**.
5. `rtcw_shaderprobe`: ejecutable independiente de inspección de etapas. `rtcw_render` conserva sus rutas API previas; los 15 programas v0.15 también se mantienen.

## Compilar

En MSYS2 mingw32/Windows (no verificado nativamente en esta entrega):

```bat
BUILD_WINDOWS.bat
```

En Linux, con GCC o Clang:

```sh
make CC=gcc -j2
make CC=clang -j2
```

Ambos compilan 16 herramientas con `-std=c89 -Wall -Wextra -Wpedantic -Werror`.

## Verificar shaders

```bat
rtcw_shaderprobe.exe scripts/monsters.shader models/creature/skin
```

La salida TSV contiene `found`, `blend`, `alpha_func`, `stages`, `unhandled`. Enumeraciones:

- Blend 0: automático (alfa simple); 1: mezcla alfa; 2: suma; 3: filtro multiplicativo.
- Alpha 0: ninguna; 1: GT0; 2: GE128; 3: LT128.
- `unhandled` > 0: el shader contiene detalles o etapas que **no se representan fielmente**.

Para previsualizar una criatura completa entre distintos PK3:

```bat
rtcw_rig.exe run-vfs mounts.txt models/players/example/body.mds scenario.txt rig.txt salida
```

Las rutas son ejemplos; deben sustituirse por archivos presentes en tus paquetes del juego.

## Límites de fidelidad y robustez

- **No hay interpretación completa de shaders**, por ejemplo `tcMod`, `rgbGen`, `alphaGen`, `depthFunc`, `depthWrite`, deformadores, mapas de luz, etapas posteriores, sorting explícito del motor, ni todos los factores OpenGL. Solo se representa el comportamiento básico de la primera etapa. Los modos no reconocidos se contabilizan pero usan previsualización limitada.
- El orden se aproxima **por centroide de triángulo** en una cámara ortográfica fija. NO es ordenamiento por píxel, partición de polígonos, ni OIT; los triángulos que se cruzan pueden quedar incorrectos. La opción `filter` representa una aproximación sobre píxeles RGBA decodificados.
- Cuando se usa una skin y un shader multi-PK3, se representa la textura ganadora ya localizada por el VFS, no todas las etapas. Solo hay 4 entradas fijas de caché; el atlas puede provocar relecturas si usa más de 4 imágenes distintas.
- Capacidad: `RTCW_MAX_TRIANGLES` 262 144; superficies 32; textura 1024 × 1024; 512 KiB por script de shader. El almacenamiento es estático. La vista PPM tiene 640 × 640 píxeles.
- Validado con **fixtures sintéticos**, no con modelos comerciales reales; Win32 MSYS2 nativo todavía requiere prueba.
- Las fuentes RTCW-SP y demás repositorios compartidos se usaron para estudiar el comportamiento; no se copiaron fuentes GPL ajenas a este paquete CC0.

## Pruebas nuevas

```sh
python3 tests/test_codec_v16.py ./rtcw_shaderprobe
```

Compueba que dos triángulos semitransparentes rendericen igual con superficies en ambos órdenes, composición alfa 128, `blendFunc`, `alphaFunc`, shader con una etapa extra, mapa VFS opcional de cuatro columnas, compatibilidad con tres columnas, y rechazo de shader mal formado. También se ejecutaron las 16 suites heredadas.
