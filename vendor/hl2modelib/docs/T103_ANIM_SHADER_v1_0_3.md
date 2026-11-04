# hl2modelib v1.0.3 — animaciones MDL + material FX (Source 48/49)

## Qué se añadió

**Nuevas unidades vendorizadas** (ANSI C89, 16.16 fixed-point, buffers suministrados por el llamador):

- `src/hl2m_native_anim89.c` decodifica las animaciones internas (mstudioanim RLE de rotación/traslación, quaternions 48/64 bits), recompone matrices jerárquicas y expone `poseToBone` original del archivo MDL. No inventa poses.
- `src/hl2m_shader89.c` interpreta material VMT y permite sombrear un fragmento RGBA mediante base, alpha-test, normal mapping tangente, Phong, rim light, detail map y contribución de environment map. Aproximación portable: **NO replica todas las rutas GPU/shader de Valve**. El presenter suministra texturas, light/view/half vectors y bases tangentes.
- `examples/hl2m_export_animated_obj89.c` ensambla MDL+VVD+VTX, lee una animación y su fotograma, deforma en CPU por pesos de huesos, genera OBJ+MTL.
- `examples/hl2m_vmt_fx_probe89.c` inspecciona efectos reales VMT.
- `examples/hl2m_shader_demo89.c` muestra tres superficies de prueba con el sombreador C89: iluminación básica, bump y bump+Phong+rim. **Son superficies sintéticas**, no una vista materializada del T-103.
- `tests/test_shader89.c`, `tests/test_native_t103.c` (esta última se ejecuta con el MDL proporcionado, que se distribuye aparte del código).

Las correcciones anteriores (Source 48/49, lectura de geometría, normal/UV, cabeceras desplazadas, VTF DXT1/DXT5, MTL/OBJ) se conservan. Además se corrigió el uso de IDs de hueso locales de VTX como si fuesen IDs globales: para skinning debe conservarse la atribución global original de VVD y resolver por la tabla apropiada para el draw/strip.

## Compilación

```sh
make clean
make -j2 CFLAGS='-std=c89 -Wall -Wextra -Werror -pedantic -Iinclude'
make check CFLAGS='-std=c89 -Wall -Wextra -Werror -pedantic -Iinclude'
```

Build con MSYS2 MINGW32: `mingw32-make CC=gcc` (o `make CC=gcc` con MSYS2). No requiere VTK, Python, OpenGL ni SDK de Valve al compilar la biblioteca. Ningún módulo nuevo usa `malloc`, `free`, `float`, `double` o enteros de 64 bits.

## Animación del corpus T103

Ejemplo de ruta relativa al corpus original del usuario:

```sh
ROOT='tyrant/models/capcom/re - dc/monsters'
./hl2m_export_animated_obj89 \
  "$ROOT/tyrantphase1.mdl" \
  "$ROOT/tyrantphase1.vvd" \
  "$ROOT/tyrantphase1.dx90.vtx" \
  1 14 walk_014.obj
```

Esto selecciona **índice interno de animación 1** (`Walk_F`) y fotograma **14** (contando desde cero). Genera `walk_014.obj` y `walk_014.mtl`. Coloca PNG homónimos a los materiales en la misma carpeta para visualizarlos. La biblioteca lee también índices desde una secuencia por `hl2m_native_sequence_animation89()` (selección de una celda de blending), pero el exporter de ejemplo recibe directamente un índice de animación. No se debe confundir índice de secuencia con índice de animación.

Para usarlo directamente en un engine:

```text
MDL + VVD + VTX
  -> hl2m_decode_mdl/vvd/vtx
  -> hl2m_build_engine_model
  -> hl2m_native_inverse_bind89
  -> hl2m_native_sample_anim_matrices89(anim, frame)
  -> hl2m_engine_cpu_skin(base, inverse_bind, animated_world)
  -> malla deformada + materiales
  -> presenter propio (OpenGL, GDI, DX9, etc.)
```

La API de biblioteca no necesita OBJ para el runtime: el exporter es solamente un programa de comprobación. Las poses se leen por fotograma entero. `hl2m_native_sample_anim_frame89` devuelve una pose Euler (limitaciones con quaternions crudos); para MDL reales con `RAWROT`/`RAWROT2` usar **`hl2m_native_sample_anim_matrices89`**.

**Prueba realizada**: las fases 1, 2 y 3 del corpus decodificaron sus huesos y deformaron respectivamente 22 671, 34 746 y 64 830 vértices de dibujo, sin referencias a huesos faltantes en las muestras inspeccionadas. `Walk_F` de la fase 1: 57 fotogramas, 57 canales óseos, 29 rotaciones quaternion comprimidas. Se generaron previews de varios fotogramas, además de una pose por fase. Estas comprobaciones **no equivalen a validar todas las animaciones de todos los archivos**.

## Efectos VMT y shading

```text
VMT -> hl2m_decode_vmt -> hl2m_vmt_to_fx89 -> hl2m_material_fx89
                                                         |
VTF decodificado -> RGBA, normal RGBA, detail RGBA, env RGBA
                                                         |
normales, tangentes, bitangentes, cámara y luz del presenter
                                                         |
                         hl2m_shade_fragment89() -> RGBA
```

El extractor real de `tyrant01/.../ety1_wear.vmt` detectó `$bumpmap`, `$alphatest` y `$nocull` (y no activó Phong/rim artificialmente). La configuración de Phong/rim se probó con un VMT sintético controlado. Por tanto **no hay evidencia de que todos los materiales originales del T-103 tengan Phong/rim**.

El demo muestra una superficie matemática/procedural para comprobar que los parámetros afectan los colores de salida. **Las capturas de T-103 suministradas en el paquete de previews son comprobaciones de animación y texturas difusas, sin el material FX C89 aplicado a cada píxel.** El renderizador de ejemplo externo (VTK) no forma parte del código vendido.

## Límites actuales (no afirmar soporte inexistente)

- No está implementada la interpolación temporal con blending continuo entre fotogramas/animaciones, blend-space 2D completo, root motion, IK final ni capas de movimiento de Valve en animaciones MDL nativas.
- Bloques de animación externos `.ani` y secciones complejas no se han verificado con T-103; algunas variantes pueden devolver explícitamente error de formato/no soportado.
- Normal maps: el presenter debe generar/cargar tangente, bitangente, normal map y datos de luz, y pasar los valores `have_*`. `normal_dxt5nm` es explícito y no se activa por adivinanza. No se implementan cascadas GPU reales, cubemaps físicos, shadows, projected lights, ni bloom.
- La decodificación/normalización fixed-point introduce pequeñas diferencias numéricas de la implementación original de Valve.
- El repositorio de SDK se usó como **referencia de estructura**. El ZIP de código no incorpora el SDK, binarios ni assets de terceros. Los modelos del corpus aportado se distribuyen sólo en el ZIP de previews exportados.
