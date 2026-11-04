# Changelog: StudioHalfLife Plus

## Added

- Added `hlfp_studiohalflife_plus.h/.c`.
- Added `hlfp_softmdl_plus.h/.c`.
- Added Xash model extra analyzer for `blendvertinfoindex` and `blendnorminfoindex`.
- Added raw boneweight table reader/normalizer.
- Added root-motion extraction.
- Added hitbox world AABB builder.
- Added two-bone IK lite target mover.
- Added LOD selection helper.
- Added audit report writer.
- Added richer QC scaffold writer.
- Added affine textured software renderer with TGA output.
- Added `test_studiohalflife_plus.c`.
- Added `demo_render_studiohalflife_plus.c`.

## Build

- Updated `Makefile` to include the new modules and tests.
- Verified with `gcc -std=c89 -Wall -Wextra -pedantic -O2`.
