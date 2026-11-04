# Built-in image decoders (`image89`) - advanced pass

`model_runtime89` decodes model textures without requiring WIC, OpenGL, Direct3D,
SDL, stb_image, libpng, libjpeg or a system zlib. All decoder vendors are C89,
no-heap, and enabled by default. A build can remove each image codec and a runtime
mask can disable any codec that was compiled in.

## Architecture

```text
model provider
   |
   +-- provider-native texture decoder (if the model format owns one)
   |
   +-- embedded encoded image ----+
   |                               |
   `-- external texture path ------+--> image89 registry
                                        |
                                        +-- PNG89  -> zlib89
                                        +-- BMP89
                                        +-- DDS89
                                        +-- TGA89
                                        +-- JPG89
                                        +-- TIM89
                                        `-- TIFF89 -> zlib89 for Deflate
                                                     |
                                                     `--> RGBA view
```

The renderer/backend only consumes pixels. It is not required to understand PNG,
JPEG, DDS, TIM, TIFF, etc.

## Native subresources

The advanced pass keeps container structure visible instead of flattening every
image to a single bitmap. `modelrt_image_info` now exposes:

```c
subresource_count
mip_count
face_count
page_count
palette_count
```

and callers can query/decode a native member:

```c
modelrt_image_subresource_info si;
modelrt_image_info ii;

modelrt_image_subresource_info_memory(&rt, encoded, encoded_size, index, &si);
modelrt_image_decode_subresource_memory(&rt, encoded, encoded_size, index,
                                        rgba, rgba_capacity, &ii, &si);
```

Mappings:

- DDS: `surface-major, mip-minor`. A six-face cube with four levels has 24 subresources.
- indexed TIM: one subresource per CLUT palette.
- TIFF: one subresource per IFD/page.
- ordinary 2D formats: subresource 0.

The legacy/default decode APIs still decode subresource 0, preserving existing callers.

## Decoder scope

| Sublib | Current scope |
|---|---|
| `zlib89` | zlib wrapper + raw DEFLATE; stored/fixed/dynamic Huffman; LZ backreferences; Adler-32 |
| `png89` | PNG filters 0..4; grayscale/RGB/indexed/GA/RGBA; supported legal 1/2/4/8/16-bit combinations; PLTE/tRNS; non-interlaced and Adam7 interlace method 1 with seven separately filtered reduced passes |
| `bmp89` | BITMAPINFO family, palettes, BI_RGB/BI_BITFIELDS, 1/4/8/16/24/32 bpp, top-down/bottom-up |
| `dds89` | uncompressed masks plus DXT1/DXT2/DXT3/DXT4/DXT5 and common DX10 BC1/BC2/BC3/RGBA8/BGRA8/BGRX8; complete 2D mip hierarchy; legacy cube faces and DX10 cube arrays; arbitrary subresource decode |
| `tga89` | indexed/truecolor/grayscale, raw and RLE types, origin/orientation handling |
| `jpg89` | 8-bit DCT Huffman sequential SOF0 and progressive SOF2; multi-scan DC/AC first/refinement, EOB runs, restart intervals, common grayscale and 3-component subsampling; integer IDCT |
| `tim89` | standard PlayStation TIM 4/8/16/24-bit; all CLUT palettes selectable; VRAM image/CLUT origins exposed; A1B5G5R5 STP transparency/semitransparency mapped to RGBA |
| `tiff89` | classic TIFF/IFD chain; multipage; strips or tiles; chunky PlanarConfiguration=1 and planar-separate=2; grayscale/RGB/RGBA/indexed 8-bit; raw, PackBits, LZW, Deflate; horizontal predictor |

## Deliberate boundaries

This pass implements the requested container structures but still does not claim
all historical extensions. JPEG arithmetic/lossless/hierarchical and CMYK colour
management are not implemented. DDS BC4-BC7/volume-texture decode is not yet in
the generic decoder. TIM Type 4 "Mixed" cannot be interpreted generically because
the flag explicitly permits mixed pixel depths in the same VRAM payload. TIFF is
classic 32-bit-offset TIFF, not BigTIFF, and does not yet implement CCITT/JPEG-in-TIFF
or arbitrary sample depths.

## Memory contract

No decoder allocates. The caller owns encoded bytes and destination/scratch space.
PNG, JPEG and TIFF may use unused tail space in the RGBA destination as temporary
storage. Progressive JPEG in particular retains quantized DCT coefficients until
all scans are complete, so its required capacity can be greater than `width*height*4`.
Insufficient capacity returns `MODELRT_ERR_NO_SPACE`.

## Compile/run controls

All are `1` by default:

```make
IMAGE_PNG=1
IMAGE_BMP=1
IMAGE_DDS=1
IMAGE_TGA=1
IMAGE_JPG=1
IMAGE_TIM=1
IMAGE_TIFF=1
```

At runtime:

```c
modelrt_runtime_set_image_decoder_mask(&rt,
    MODELRT_IMAGE_PNG | MODELRT_IMAGE_DDS | MODELRT_IMAGE_JPG |
    MODELRT_IMAGE_TIM | MODELRT_IMAGE_TIFF);
```

## Regression targets

```sh
make image-codec-regression
make image-codec-advanced-regression
```

The advanced fixtures cover a real Adam7 stream, a 10-scan progressive JPEG,
a six-face/four-mip DDS cube, a two-palette TIM, tiled+Deflate TIFF,
planar-separate TIFF and a two-page TIFF.
