# Image codec vendorization — v0.12

## Architecture and limits

- `src/c4d89_img_png.c`: standalone PNG scanner (CRC-32, PLTE, tRNS, filters 0–4), zlib RFC1950 and DEFLATE RFC1951 reader (stored, fixed and dynamic Huffman). PNG 8-bit, grayscale/RGB/indexed/gray+alpha/RGBA. Non-interlaced only; no 16-bit or Adam7.
- `src/c4d89_img_tga.c`: TGA 8/16/24/32-bit, color mapped, RGB and grayscale; raw and RLE; origin flags, 1-bit alpha on 16-bit images.
- `src/c4d89_img_bmp.c`: BMP 8-bit indexed, 16-bit RGB555/bitfields, 24-bit BGR and 32-bit RGB/bitfields; top-down/bottom-up. No RLE4/RLE8. Unspecified 32-bit BI_RGB alpha is deliberately considered opaque, whereas an explicit alpha bitmask is honored.
- `src/c4d89_img_jpeg.c`: baseline sequential JPEG (SOF0), 8-bit grayscale and YCbCr; H1V1/H1V2/H2V1/H2V2 sampling, Huffman, restart markers, integer IDCT. No progressive, arithmetic-coded JPEG, CMYK or lossless JPEG. Integer IDCT and nearest-neighbor chroma sampling can differ slightly from Pillow/libjpeg.
- `src/c4d89_img_dispatch.c`: chooses decoder from actual bytes and loads directly from a C4D embedded JPEG offset or an external asset.
- `src/c4d89_img_resource.c`: connects existing C4D89 ambiguity-safe external bitmap resource resolver to the same RGBA decoder through an engine VFS callback.

All modules emit **RGBA8 unpremultiplied, top-left origin**, row-major `width * height * 4` bytes, with no automatic mipmaps or GPU side effects. The engine keeps ownership of the output buffer.

**No dynamic allocations:** no `malloc/free`; internal state is static, and the output is supplied by the caller. This choice gives fixed maximums and no concurrency. Use an external lock or independent instances if multiple threads must decode concurrently.

Limits: max image dimension 4096 pixels; max source-file input 16 MiB; PNG compressed scratch 16 MiB and decompressed scratch 32 MiB; caller must supply enough RGBA output space. The code uses integers and masks; no floating point or explicit 64-bit values. Static buffers can be adjusted via source constants for smaller targets.

## Call directly from any engine

```c
#include "c4d89_image.h"
static unsigned char rgba[512ul * 512ul * 4ul];
C4D89_ImgInfo img;
int status = c4d89_img_decode_file("tex/character.tga",rgba,sizeof(rgba),&img);
if (status == C4D89_IMG_OK) {
    /* Upload rgba, img.width and img.height to engine's GPU/backend. */
}
```

Or decode already loaded bytes: `c4d89_img_decode(src, bytes, rgba, cap, &img)`.

### Embedded JPEG without intermediary file

```c
int status = c4d89_img_decode_embedded(
  "character.c4d", jpeg_offset, jpeg_size, rgba, sizeof(rgba), &img);
```

### External bitmap via engine VFS

Given a `C4D89_ResourceIndex` (existing type in c4d89.h), implement callback:

```c
int reader(void *user,const char *normalized_relative_path,
           const unsigned char **data,unsigned long *bytes);
```

Then:

```c
int status = c4d89_img_decode_resource(
    "characters/hero.c4d", "HeroBody.tga", &resource_index,
    reader, user, rgba, sizeof(rgba), &img);
```

This uses the original asset matcher to reject ambiguous paths; **the renderer decides which material/selection requests which bitmap**. The module does not pretend to derive the remaining proprietary C4D Material BaseLink.

## Build

```sh
./scripts/build_image_codecs.sh       # libc4d89_images.a and c4d89_imgtool
./scripts/build_vendor_library.sh     # all codecs (legacy and image) in libc4d89.a
./scripts/test_image_codecs.sh        # C89 VFS callback and pixel regression
```

Recommended GCC/Clang options: `-std=c89 -pedantic -Wall -Wextra -Werror`.

## Evidence and QA (v0.12)

- 14 distinct reference image/format tests, including paletted PNG/TGA/BMP, TGA RLE, PNG RGBA and grayscale/baseline JPEG.
- PNG, BMP, TGA RGB tests produce **exact** reference pixels. JPEG tests show expected small IDCT/chroma-sampling differences.
- 12 corpus JPEG files recovered from FemaleHobo and MalePunk (.c4d embedded JPEGs previously extracted) decoded in 90×90 and 256×256 versions. Full-size textured atlas tests differ from Pillow by ~0–1.1 RGB levels mean absolute error.
- Damaged PNG CRC and truncated JPEG are rejected; progressive JPEG rejects as unsupported.
- No claim that all PNG/JPEG variants on the internet are supported, or that the native `.c4d` material-link field has been solved.
