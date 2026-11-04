# RTCW Monster Codec v0.13 — Animated visibility and textured contact sheets

**Entrega:** fuentes C89 de autoría original, CC0-1.0, sin recursos comerciales ni copias del motor RTCW.

## Novedades

- `rtcw_visibility.c/.h`: tabla de visibilidad por superficie con mapa explícito de piezas `PART:0..7`; compacta **triángulos**, no vértices. Mantiene tags y jerarquía de accesorios ocultos.
- `rtcw_rig_main.c`: instrucciones opcionales `ROOT_PART N`, `TEXTURE_ROOT path`, `SHADER_FILE file` y `ATTACH ... PART:N` en el rig. La selección de comandos sigue siendo explícita, sin ejecutar la IA del juego.
- `rtcw_contact_color.c/.h`: genera hojas de contacto `contact_color_000.ppm`, P6, 640x480, 4 columnas x 3 filas de imágenes de 160x160; conserva la hoja alámbrica existente de 800x600.
- `rtcw_texrender.c`: se reutiliza el visor fijo 640x640, texturizado con TGA y selección de materiales `map`/`clampmap`/primera imagen de `animMap`. Material desconocido -> damero de diagnóstico; el conteo queda en `timeline.tsv`.

## Protocolo C89

- `-std=c89 -Wall -Wextra -Wpedantic -Werror`.
- Sin `malloc`, `calloc`, `realloc`, `free`, heap, `float`, `double` ni `long long` en el nuevo código.
- Búferes globales con límites estrictos (igual que versiones anteriores), tamaños de salida acotados.
- Sin dependencias externas de imagen/ZIP. El renderer de materiales es una **aproximación offline**, no reproduce el pipeline completo de RTCW.

## Compilar

```bat
BUILD_WINDOWS.bat
```

En Linux/Clang: `make CC=clang all`, o `make all` para GCC. **Compilación nativa MSYS2 Win32 no comprobada** en esta entrega; necesita verificarse en tu equipo.

## Ejemplo de plan de rig

```text
ROOT_PART 0
ROOT_SKIN C:/RTCW/assets/models/monsters/foo/body.skin
TEXTURE_ROOT C:/RTCW/assets
SHADER_FILE C:/RTCW/assets/scripts/monsters.shader
ATTACH tag_head C:/RTCW/assets/models/monsters/foo/head.md3 TORSO - PART:1
ATTACH child_tag_eyes C:/RTCW/assets/models/monsters/foo/eyes.md3 FIXED:0 - PART:2
```

Las rutas y nombres de tags anteriores son **ejemplos**, no archivos confirmados del juego. Para ejecutar:

```bat
rtcw_rig.exe run body.mds wolfanim.cfg wolfanim.script scenario.txt rig.txt NEW_OUTPUT_DIR
```

O en modo VFS:

```bat
rtcw_rig.exe run-vfs mounts.txt models/players/example/body.mds scenario.txt rig.txt NEW_OUTPUT_DIR
```

En VFS, `ATTACH` y `ROOT_SKIN` aceptan `@ruta/virtual` para resolver archivos mediante los PK3 montados. `SHADER_FILE` también acepta `@ruta/virtual` en VFS. **`TEXTURE_ROOT` debe apuntar a una carpeta local con los TGA extraídos previamente**, incluso en modo VFS. No descarga automáticamente los materiales del VFS.

`TEXTURE_ROOT`, `SHADER_FILE`, `ROOT_SKIN` y nombres de archivos en el plan de rig se analizan como tokens sin espacios; utiliza rutas sin espacios en esta versión.

## Asociación de partes

- Una superficie no asociada a `PART:N` siempre permanece visible.
- `PART:0` a `PART:7` son una asignación **explícita del usuario**: los números no se infieren automáticamente del modelo ni se garantiza que representen la organización exacta del juego.
- `showpart N` en un comando seleccionado marca el bit N como visible; `hidepart N` lo desmarca.
- El bit inicial de visibilidad es 255 (las ocho partes visibles). `ROOT_PART N` asigna las superficies del cuerpo principal a un bit; si se omite, el cuerpo permanece visible.
- Las partes ocultas siguen existiendo internamente para dar soporte a tags descendientes; solamente se excluyen sus triángulos de OBJ, PPM alámbrica y render a color.
- No se hacen inferencias sobre acciones de IA, nombres de monstruos ni semántica original por enemigo.

## Productos

- `pose_NNN.obj`: geometría visible tras filtrado de triángulos.
- `pose_NNN.ppm`: vista alámbrica PPM P6 de 800x800.
- `pose_NNN_color.ppm`: vista texturizada PPM P6 de 640x640, si `TEXTURE_ROOT` se especificó.
- `contact_NNN.ppm`: mosaico alámbrico de 800x600.
- `contact_color_NNN.ppm`: mosaico texturizado de 640x480, opcional.
- `timeline.tsv`: incluye `hidden_triangles` y `missing_textures` (nuevas columnas), además de los datos de la v0.12.
- `actions.tsv` y `graph.tsv`: trazabilidad del script sin ejecutar reglas de IA.

## Limitaciones y prioridades

- La lectura de texturas es TGA: **no JPEG**, por lo que algunos materiales del juego aparecerán como damero aunque existan imágenes JPEG.
- No soporta multi-pass shaders, blending exacto, tcMods, deformVertexes, iluminación ni reproducción temporal de `animMap`.
- El renderizador usa un encuadre automático aproximado y UV afines, no un equivalente al renderer original.
- La aplicación de la máscara por piezas es un mapeo de previsualización. El código original contiene reglas específicas de accesorios que todavía no se han reconstruido completamente.
- Sin ensayos con PK3 comerciales/monstruos reales. No hay validación nativa del ejecutable en Windows/MinGW.
- La fase natural siguiente es integrar la resolución automática de TGA y shaders entre varios PK3, además de pruebas de modelos reales con paridad de clips y partes.

## Pruebas

```sh
python3 tests/test_codec_v12.py ./rtcw_rig
python3 tests/test_codec_v13.py ./rtcw_rig
```

Las trece suites se ejecutaron en Linux con fixtures sintéticos. Las nuevas pruebas cubren ocultación y restauración, OBJ, color, hojas de contacto, conteos de materiales, mapeo explícito, errores de rig y compatibilidad sin texturas. El ejecutable `rtcw_rig` también pasó con Clang + UBSan.
