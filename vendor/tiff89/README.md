# TIFF89
C89 no-heap TIFF-to-RGBA decoder for a bounded strip/chunky 8-bit subset, including raw, PackBits, LZW and Deflate. Deflate is decoded by vendored `zlib89`. Current scope is documented in `../../IMAGE_DECODERS89.md`.

## Advanced runtime pass
Traverses the classic IFD chain for multipage TIFF and supports strip/tile storage
with PlanarConfiguration 1 and 2 for the supported 8-bit photometric families.
