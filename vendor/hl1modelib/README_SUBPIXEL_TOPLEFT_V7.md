# hl1modelib — Subpixel / Top-left v7 (C89)

**Cambio aditivo sobre v6:** `hlfp_subpixel89.c/.h` y el selector optativo
`hlfp_soft2_options.subpixel_top_left` (inicialmente `HLFP_FALSE`).
No se elimina ningun codec ni se cambia la ruta predeterminada de v6.

## Que cambia

- Vertices proyectados en **28.4**: 1/16 de pixel, sin `float`, `double`, `malloc`, `free` ni `long long` en el nuevo modulo.
- Muestras en centros de pixel `(x*16+8, y*16+8)`.
- Convencion **top-left** normalizada con independencia del winding.
  La arista compartida pertenece a una sola cara, nunca a las dos.
- Coordenadas de proyeccion 16.16 cuantizadas a 28.4 antes del raster,
  tambien al usar perspectiva y clipping de seis planos.
- Los numeradores baricéntricos se reutilizan en las interpolaciones existentes de
  coordenadas UV, luz Gouraud y profundidad.
- Los overlays de lineas/esqueleto permanecen en su rasterizado anterior.

## Activacion

```c
hlfp_soft2_options opts;
hlfp_soft2_options_init(&opts);
opts.subpixel_top_left = HLFP_TRUE;  /* modo v7 */
opts.wire_overlay = HLFP_FALSE;
/* Llamar hlfp_soft2_draw_expanded_mesh_textured(..., &opts, ...). */
```

Modo avanzado: `opts.perspective = &camera; opts.full_frustum_clipping = HLFP_TRUE;`.
Es responsabilidad del motor facilitar framebuffer y buffers de profundidad.
No se realiza asignacion dinamica dentro de la nueva ruta.

## Compilar y probar (GCC o MSYS2 MinGW32)

```
make clean
make CC=gcc AR=ar CFLAGS="-std=c89 -Wall -Wextra -Werror -pedantic -O2" all corpus_probe_v7
./test_hlfp_v7
./test_hlfp_v6
./test_hlfp_v5
./demo_subpixel_v7 PREVIEWS_V7/coverage
```

**Resultados de esta entrega:** `test_hlfp_v7` paso 279610 aserciones;
cuadrado 144/144, cero huecos y cero solapamientos.
La prueba de orden de dibujo dio 34 diferencias de bytes con v6 y 0 con v7.
El probe v7 proceso 834 archivos MDL principales: 834 OK; 822
con geometria y secuencias, 12 sin esas dos condiciones. Probe a 12x12,
por lo que NO equivale a una inspeccion visual completa por modelo.
Se guardo una comparacion de 768x768 del zombie de Cold Blood:
62947 pixeles RGB distintos de la v6, sin afirmar equivalencia pixel a pixel con GoldSrc.

## Limites documentados

1. Para garantizar calculos de aristas sin overflow en un C89 de 32 bits,
   `hlfp_sp89_prepare` retorna `HLFP_ERR_OVERFLOW` si el area potencial excede
   el margen de calculo seguro. **No** cambia silenciosamente a aritmetica
   imprecisa. Triangulos muy grandes, como algunos que cubren un viewport 4K,
   pueden encontrar este limite; se debera dividir por tiles en una fase posterior.
2. Top-left es una convencion de cobertura coherente, pero **no** demuestra
   que Half-Life/GoldSrc original use identica convencion para cada driver.
3. Persisten aproximaciones previas en chrome, materiales y transparencias
   superpuestas. Anti-aliasing/MSAA y derivadas mipmap no forman parte de v7.
4. La ruta de lineas usa Bresenham entero; el selector v7 afecta a triangulos
   texturizados y a sus mallas, no la cobertura de lines en modo wireframe.
5. Se verifico GCC/Linux; la prueba especifica con compilador MinGW32 no se pudo
   ejecutar en este entorno.

## Archivos nuevos

- `hlfp_subpixel89.c/.h`: algoritmo portable sin memoria dinamica.
- `test_hlfp_v7.c`: comparticion exacta de borde, winding, T-junction,
  desplazamientos subpixel, overflow, prueba end-to-end de orden.
- `demo_subpixel_v7.c`: capturas lado a lado con geometria real del rasterizador.
- `corpus_probe_v7.c`: probe real GoldSrc con selector v7 activo.
- `demo_render_goldsrc_v7.c`: preview de modelos `v6|v7` en el mismo ejecutable.
- `REPORT_V7_CORPUS_ALL.csv`, `PREVIEWS_V7`, `BUILD_V7_STRICT_C89.log`
  y `TESTS_V7_ALL.log`.
