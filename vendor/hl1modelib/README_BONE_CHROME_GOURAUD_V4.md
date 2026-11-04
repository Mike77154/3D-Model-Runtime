# hl1modelib v4 — GoldSrc Bone Chrome & Vertex Gouraud (C89)

Actualización acumulativa sobre v3. Ningún módulo original ha sido eliminado. El nuevo módulo `hlfp_goldsrc_shade89.c/.h` contiene las operaciones de chrome por hueso e intensidad hemisférica por normal. La rasterización ahora permite interpolar los factores por píxel **después de calcular una iluminación para cada vértice**. Es una aproximación fixed-point inspirada en Valve `StudioModel::Chrome` y `StudioModel::Lighting`, no equivalencia píxel a píxel con el renderer OpenGL de GoldSrc.

## APIs y activación

Las opciones nuevas **desactivadas por defecto** son:

- `opt.chrome_per_bone=HLFP_TRUE`: utiliza `normal_bone` de cada vértice expandido, la traslación del hueso en la pose activa, `chrome_eye`, y `chrome_view_right` para calcular ejes reflectivos que cambian con el hueso y la posición del observador. Al trabajar ya con normales transformadas a mundo, las proyecciones son equivalentes conceptualmente a invertir la rotación del hueso para calcular coordenadas en espacio local. Se usan normales L2 normalizadas en fixed-point y un fallback para colinealidad del observador y el eje derecho.
- `opt.smooth_vertex_light=HLFP_TRUE`: evalúa una intensidad GoldSrc/Lambert sobre cada normal transformada y la interpola barycéntricamente en el triángulo (Gouraud). Respeta `NF_FULLBRIGHT` y `NF_FLATSHADE`.
- `opt.light_lambert=HLFP_FIXED_ONE+HLFP_FIXED_ONE/2`: hemisferio 1.5 16.16, con mínimo 1.0. `light_direction` se interpreta como **vector de luz GoldSrc hacia la escena, no surface-to-light** al activar esta ruta (convención de la rutina original), por lo que el contraste puede ser inverso al shader direccional v3.
- `opt.bone_matrices` y `opt.bone_matrix_count`: para llamadas independientes por malla, puedes aportar el arreglo de matrices *world* de la pose. Los compositores `hlfp_soft2_render_mdl_sequence_subframe_textured` y `hlfp_soft3_render_mdl_sequence_subframe_textured` lo suministran automáticamente a partir del scratch, sin asignaciones dinámicas. Estas referencias sólo viven durante la llamada.
- `opt.chrome_view_right`: dirección de la derecha de cámara en coordenadas del modelo, necesaria para la orientación del mapeo chrome (por defecto +X).

Para probarlo en un compositor:

```c
hlfp_soft2_options opts;
hlfp_soft2_options_init(&opts);
opts.chrome_per_bone=HLFP_TRUE;
opts.smooth_vertex_light=HLFP_TRUE;
opts.chrome_view_dependent=HLFP_TRUE;
opts.wire_overlay=HLFP_FALSE;
opts.skeleton_overlay=HLFP_FALSE;
opts.chrome_eye.z=128*HLFP_FIXED_ONE;
opts.light_direction.z=-HLFP_FIXED_ONE;
/* llamar a hlfp_soft3_render_mdl_sequence_subframe_textured(...) */
```

**Compatibilidad:** el código antiguo sigue compilando, aunque agregar campos a `hlfp_soft2_options` modifica su tamaño en memoria: debes **recompilar** el motor y la biblioteca, sin mezclar binarios de la v3 y v4. No hay `malloc/free`, `float/double` ni enteros explícitos de 64 bits en la nueva unidad.

## Compilar y probar (GCC/MSYS2)

```bash
make clean
make CC=gcc AR=ar CFLAGS='-std=c89 -Wall -Wextra -Werror -pedantic -O2' all corpus_probe_v4
./test_hlfp_v4
./test_hlfp_v3
./test_hlfp_material89
./corpus_probe_v4 '/ruta/al/modelo.mdl' tiny
./demo_render_goldsrc_v4 '/ruta/al/modelo.mdl' /tmp/base 0 0
./demo_render_goldsrc_v4 '/ruta/al/modelo.mdl' /tmp/nuevo 0 0 v4
```

## Pruebas realizadas aquí

- GCC/Linux, strict C89, `-Wall -Wextra -Werror -pedantic -O2`, sin warnings de compilación.
- Diez ejecutables de pruebas: `test_hlfp`, `test_hlfp_goldsrc`, `test_studiohalflife`, `test_studiohalflife_plus`, `test_studiohalflife_ultra`, `test_hlfp_corpusfix`, `test_hlfp_timeline`, `test_hlfp_material89`, `test_hlfp_v3`, `test_hlfp_v4`, todos con salida de éxito.
- Corpus 822/822 archivos `.mdl` (mismos modelos v3), render de 12×12 para primera secuencia, primer fotograma fraccional, sin errores. Véase `REPORT_V4_CORPUS_ALL.csv`. **Smoke test**, no revisión de todas las secuencias ni certificación de textura exacta.
- Renders de 768×768 de `SecurityKey.mdl` (chrome), `flashlight_on.mdl` (chrome/luces) y `zombie.mdl` (texturas). Cambios de píxel visibles respecto a v3, sin implicar automáticamente mayor fidelidad.
- Previews `PREVIEWS_V4/hl1_v4_three_models_comparison.png` y `PREVIEWS_V4/flashlight_v3_vs_v4.png`.

## Pendientes

1. Paridad del efecto chrome contra una captura del ejecutable GoldSrc, incluidos parámetros reales `g_vright`, `m_origin` y cache de huesos. Para mallas con pesos normales múltiples, `normal_bone` se usa como referencia representativa: se necesitaría un cálculo especial para chrome ponderado.
2. Iluminación del GoldSrc vivo: color de luz, luces locales, `g_lambert` en tiempo de ejecución y condiciones de escena. Esta prueba sólo implementa una luz global escalar.
3. Antialias, clipping próximo, UV con perspectiva y transparencia intersecante (OIT) siguen pendientes.
4. No se dispone aquí de MSYS2/MinGW32 para ejecución nativa, por lo que se requiere validación de Windows por separado.

Referencia primaria: `utils/mdlviewer/studio_render.cpp` del SDK de Valve incluido en el corpus y https://github.com/ValveSoftware/halflife/blob/master/utils/mdlviewer/studio_render.cpp.
