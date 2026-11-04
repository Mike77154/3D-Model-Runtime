# Giffany v13 — KIB Decomp + Paeth/Wavelet Push

Esta pasada empuja la ruta de texturas de Genesis3D más abajo: `GeBm` ahora no solo se puede probar y convertir a RGBA/TGA cuando viene raw, también puede desempaquetar mips comprimidos por una sub-lib propia, revertir Paeth y abrir una ruta experimental para wavelet 8-bit.

## Referencias locales revisadas

Del source de Genesis3D entregado:

```text
Genesis3D-master/Bitmap/bitmap.c
Genesis3D-master/Bitmap/bitmap.h
Genesis3D-master/Bitmap/pixelformat.c
Genesis3D-master/Bitmap/pixelformat.h
Genesis3D-master/Bitmap/Compression/*
Genesis3D-master/VFile/vfile.h
Genesis3D-master/VFile/vfile.c
```

Hallazgos importantes usados en el parche:

- `MIP_FLAG_COMPRESSED` corresponde al bit `1 << 4` dentro del byte de mip.
- `MIP_FLAG_PAETH_FILTERED` corresponde al bit `1 << 5`.
- El código de Genesis3D 1.0 deja `GE_PIXELFORMAT_WAVELET` como no soportado en varias rutas.
- La lectura/escritura LZ está protegida por `DO_LZ`; en este árbol entregado no hay implementación pública completa de `GE_VFILE_TYPE_LZ`, así que esta versión agrega una capa propia y portable en vez de fingir compatibilidad bit-exact con un módulo ausente.

## Sub-lib nueva: `g3d_kib`

Archivos nuevos:

```text
g3d_kib.h
g3d_kib.c
```

API principal:

```c
const char *g3d_kib_method_name(g3d_u32 method);

g3d_u32 g3d_kib_container_header_size(void);
int g3d_kib_wrap_container(g3d_u32 method,
                           const g3d_u8 *payload, g3d_u32 payload_size,
                           g3d_u32 unpacked_size,
                           g3d_u8 *dst, g3d_u32 dst_size,
                           g3d_u32 *used_bytes);

int g3d_kib_decode_auto(const g3d_u8 *src, g3d_u32 src_size,
                        g3d_u8 *dst, g3d_u32 dst_size,
                        g3d_u32 flags,
                        g3d_u32 *src_used,
                        g3d_u32 *method_out);

int g3d_kib_packbits_encode(...);
int g3d_kib_packbits_decode(...);
int g3d_kib_lzss12_decode(...);

int g3d_kib_paeth_filter(...);
int g3d_kib_paeth_unfilter(...);

int g3d_kib_wavelet8_decode(...);
```

Métodos actuales:

```text
store
packbits
lzss12
wavelet8
raw-fallback
```

## Contenedor `KIB0`

Para que la compresión propia tenga tamaños fiables dentro del stream `GeBm`, se agregó un contenedor pequeño:

```text
u32 magic        'KIB0'
u8  version      1
u8  method       store/packbits/lzss12/wavelet8
u16 reserved
u32 raw_size
u32 payload_size
u8  payload[payload_size]
```

`g3d_kib_decode_auto()` prefiere `KIB0` cuando aparece. Si no hay magic, intenta LZSS12 y PackBits como fallback best-effort. Para assets generados por esta lib, `KIB0` es la ruta recomendada porque evita adivinar el tamaño comprimido.

## GeBm comprimido

`g3d_bitmap_probe()` ahora rellena metadatos nuevos:

```c
mip.stored_size;
mip.codec_method;
info.palette_stored_size;
info.palette_codec_method;
```

Flags nuevos:

```c
G3D_BITMAP_INFO_HAS_COMPRESSED_MIPS
G3D_BITMAP_INFO_HAS_PAETH_MIPS
G3D_BITMAP_INFO_HAS_COMPRESSED_PALETTE
G3D_BITMAP_INFO_WAVELET_FORMAT
```

API pública nueva:

```c
int g3d_bitmap_unpack_mip(const void *data, g3d_u32 size, g3d_u32 wanted_mip,
                          g3d_u8 *dst, g3d_u32 dst_size,
                          g3d_u32 *width_out, g3d_u32 *height_out,
                          g3d_u32 *raw_bytes_out,
                          g3d_u32 *codec_method_out);
```

Esto entrega el mip ya descomprimido y ya sin filtro Paeth, pero todavía en formato pixel crudo original. Después puede pasar por `g3d_bitmap_decode_mip_rgba32()` o por un importador propio.

## Paeth

Mips con flag `MIP_FLAG_PAETH_FILTERED` ahora pasan por:

```c
g3d_kib_paeth_unfilter(data, width, height, bytes_per_pixel);
```

La operación se hace después de descomprimir/copiar el mip. También hay `g3d_kib_paeth_filter()` para tests y para escribir fixtures canónicos.

## Paletas comprimidas

El parser de paleta `GeBm` ahora reconoce el flag de compresión de paleta y descomprime el bloque con `g3d_kib_decode_auto()`. Luego `g3d_bitmap_decode_mip_rgba32()` puede usar esa paleta para RGBA igual que con paletas raw.

## Wavelet experimental

Genesis3D 1.0 reporta wavelet como unsupported en el source entregado. Para no bloquear pipelines propios, se agregó una ruta experimental:

```text
G3D_PIXELFORMAT_WAVELET + mip compressed + KIB method wavelet8
```

El payload `wavelet8` usa:

```text
u16 width
u16 height
u8  levels
u8  reserved
s16 coefficients[width * height]
```

`levels = 0` funciona como fixture/passthrough 8-bit. `levels > 0` hace una inversa Haar sencilla para imágenes power-of-two. No pretende ser bit-exact con un codec wavelet propietario no incluido.

## CLI nuevo

```sh
./g3dtool bmpraw <bitmap_000.bin|GeBm> <out.raw> [mip]
```

Ejemplo:

```sh
./g3dtool bmpraw out_actor/bitmap_000.bin out_actor/bitmap_000.raw
```

Salida esperada:

```text
wrote out_actor/bitmap_000.raw 2x2 raw_bytes=4 mip=0 codec=store
```

## Tests nuevos

`selftest.c` ahora cubre:

- KIB container `store`.
- PackBits encode/decode.
- mip `GeBm` comprimido con KIB PackBits.
- Paeth filter/unfilter.
- mip `GeBm` Paeth-only.
- paleta comprimida con KIB.
- wavelet8 experimental.
- decode RGBA desde mips comprimidos/paletas comprimidas.

`smoke.sh` ahora también ejecuta:

```sh
./g3dtool bmpraw _smoke/extracted/bitmap_000.bin _smoke/extracted/bitmap_000.raw
```

## Límites honestos

- `DO_LZ` aparece en el source original, pero no hay implementación completa de `GE_VFILE_TYPE_LZ` dentro del árbol entregado; por eso `g3d_kib` implementa una capa propia portable.
- La ruta sin contenedor para LZSS12/PackBits es best-effort; `KIB0` es la forma robusta.
- Wavelet queda experimental y orientado a extracción/importación propia, no a reproducir un wavelet original inexistente en el source recibido.
- Paeth sí está implementado y testeado para mips raw/descomprimidos.
