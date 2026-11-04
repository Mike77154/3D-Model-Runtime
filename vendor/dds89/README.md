# DDS89
C89 no-heap DDS-to-RGBA decoder. Handles the top mip of uncompressed mask-based DDS and BC1/BC2/BC3-compatible DXT1/DXT3/DXT5 content. Current scope is documented in `../../IMAGE_DECODERS89.md`.

## Advanced runtime pass
`dds89_get_info`, `dds89_get_subresource_info`, and
`dds89_decode_subresource_rgba` expose mip/cubemap hierarchy without flattening it.
