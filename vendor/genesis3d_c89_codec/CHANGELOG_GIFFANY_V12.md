# Giffany v12 TexturePush

Empuje enfocado en cerrar la brecha entre inspeccionar texturas Genesis3D y poder sacarlas como pixels utiles para importadores, editores y pipelines externos.

## Cambios principales

- Se agrego decoder publico `GeBm -> RGBA32`:
  - `g3d_bitmap_decode_mip_rgba32(...)`
  - Soporta mips raw sin compresion.
  - Soporta color-key opcional.
  - Soporta alpha anidado opcional cuando el `GeBm` trae bitmap alpha separado.

- Se agrego decoder de textura de material:
  - `g3d_body_decode_material_rgba32(...)`
  - Permite decodificar directo desde `body->materials[i].bitmap_data`.

- Se agregaron comandos CLI:
  - `g3dtool bmp2tga <bitmap_000.bin|GeBm> <out.tga> [mip]`
  - `g3dtool materialtga <body.bdy|actor.act> <outdir> [mip]`

- `g3dtool extract` ahora intenta exportar tambien TGA para cada bitmap decodificable:
  - `bitmap_000.bin`
  - `bitmap_000.tga`

- Se ampliaron pruebas y smoke tests:
  - Decode de sample `GeBm` 8-bit gray.
  - Decode por material desde body.
  - Rechazo de mip inexistente.
  - Decode de `GeBm` palettized con paleta RGB.
  - Export `bmp2tga`.
  - Export `materialtga` desde `.bdy` y `.act`.
  - `extract` verificando TGA generado.

## Formatos raw decodificados a RGBA32

- 8-bit gray
- 8-bit palettized
- 16-bit 555 RGB/BGR
- 16-bit 565 RGB/BGR
- 16-bit 4444 ARGB
- 16-bit 1555 ARGB
- 24-bit RGB/BGR/YUV-as-RGB
- 32-bit RGBX/XRGB/BGRX/XBGR
- 32-bit RGBA/ARGB/BGRA/ABGR
- UV/UVL como false-color de inspeccion

## Limites honestos

- No se descomprime LZ, Paeth ni wavelet todavia.
- `Geometry/BODY` crudo no trae los bytes de `Bitmaps/N`; para exportar texturas se necesita el `.bdy` completo o el `.act`.
- La exportacion TGA cubre mips raw decodificables; cuando un bitmap usa compresion Genesis3D no soportada, el CLI lo reporta y lo salta.
