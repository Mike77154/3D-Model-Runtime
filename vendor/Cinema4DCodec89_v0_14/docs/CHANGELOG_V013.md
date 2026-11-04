# Cambios v0.13

### Nuevos módulos

- `src/c4d89_img_probe.c`: preflight de tamaño y memoria RGBA8 por cabeceras PNG, TGA, BMP y JPEG baseline.
- `src/c4d89_anim_frame.c`: muestreo por fotograma de todos los tracks C4D6 de stride fijo desde un único handle FILE y callbacks al motor.

### Cambios correctivos

- `src/c4d89_img_tga.c`: rechaza RLE que desborde el número declarado de píxeles; devuelve error de capacidad cuando el buffer RGBA8 no alcanza.
- `src/c4d89_img_bmp.c`: interpreta alturas negativas sin depender de la conversión de unsigned a signed fuera de rango.
- `tools/c4d89_inspect.c`: nuevo `frame` para imprimir la muestra de un instante Q16.16.
- Scripts POSIX/MinGW32 y bibliotecas estáticas incluyen los módulos nuevos.

### Pruebas

- `tests/test_v13_image_preflight.c`
- `tests/test_v13_anim_frame.c`
- `scripts/test_v13_core.sh`

### Limitaciones aún abiertas

- `TextureTag -> Material` BaseLink serializado.
- Jerarquía real padre-hijo y pesos de skinning.
- Curvas de Cinema 4D Bézier originales y animación esquelética completa.
- JPEG progresivo y PNG entrelazado/16-bit.
