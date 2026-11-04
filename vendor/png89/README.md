# PNG89
C89 no-heap PNG-to-RGBA decoder. Uses the vendored `zlib89`; it does not link system zlib/libpng. Current scope and limits are documented in `../../IMAGE_DECODERS89.md`.

## Advanced runtime pass
Supports PNG interlace method 1 (Adam7). Each of the seven reduced images is
inflated, unfiltered independently, and scattered into the final RGBA image.
