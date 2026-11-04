# JPG89
C89 no-heap baseline sequential Huffman JPEG-to-RGBA decoder with integer IDCT. Progressive/arithmetic/CMYK are not claimed. Current scope is documented in `../../IMAGE_DECODERS89.md`.

## Advanced runtime pass
Supports Huffman sequential SOF0 and Huffman progressive SOF2 with DC/AC
first/refinement scans and EOBRUN handling. Coefficients are kept in caller-owned
scratch space until the final integer IDCT.
