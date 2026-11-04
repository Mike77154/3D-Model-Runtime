# Changelog - studiohalflife extensions

## 2026-06-19

Added `studiohalflife extensions` layer:

- Added `hlfp_studiohalflife.h/.c`.
- Added `hlfp_softmdl.h/.c`.
- Added `test_studiohalflife.c`.
- Added `demo_render_studiohalflife.c`.
- Added fixed-point `studiohdr2` parsing.
- Added pose parameter parsing.
- Added extended boneinfo scan and procedural-bone-lite application.
- Added Xash-style 4-weight vertex/normal boneweight decoding.
- Added weighted mesh bake path.
- Added autolayer parser and weight evaluator.
- Added IK chain/link/lock parsing and lightweight IK target application.
- Added raw IK rule decoder.
- Added `.mdl`/`.dol` companion classifier and header validator.
- Added tiny software renderer with RGBA framebuffer, optional zbuffer and PPM output.
- Added generated preview artifacts: `studiohalflife_preview.ppm` and `studiohalflife_preview.png`.

Validation:

- `make clean all test_output.txt studiohalflife_preview.ppm`
- `gcc -std=c89 -Wall -Wextra -pedantic -O2`
- No warnings in build log.
