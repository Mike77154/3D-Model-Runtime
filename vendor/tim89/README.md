# TIM89
C89 no-heap PlayStation TIM-to-RGBA decoder for 4/8/16/24-bit modes. Indexed generic decode uses the first CLUT row. Current scope is documented in `../../IMAGE_DECODERS89.md`.

## Advanced runtime pass
All palettes in 4/8-bit TIM CLUT blocks are individually selectable. The API also
exposes image/CLUT VRAM coordinates and maps the PSX STP bit to RGBA alpha semantics.
