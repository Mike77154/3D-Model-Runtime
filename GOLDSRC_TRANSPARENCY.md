# GoldSrc Studio texture render modes

This revision exposes GoldSrc Studio texture flags through the transient
`modelrt_material_view.flags` and `modelrt_texture_info.flags` fields.

Portable runtime bits:

- `MODELRT_MAT_FLATSHADE`
- `MODELRT_MAT_CHROME`
- `MODELRT_MAT_FULLBRIGHT`
- `MODELRT_MAT_NOMIPS`
- `MODELRT_MAT_ALPHA_BLEND`
- `MODELRT_MAT_ADDITIVE`
- `MODELRT_MAT_ALPHA_MASK`

The GoldSrc provider maps the native `STUDIO_NF_*` / `HLFP_STUDIO_NF_*`
flags onto those bits. The native MDL remains authoritative; this is only a
render-semantic view, not a converted material format.

## Masked textures

GoldSrc masked Studio textures use palette index 255 as the transparent
texel. `hlfp_goldsrc_texture_to_rgba32()` already preserved that native rule:
index 255 becomes RGBA alpha 0 when `STUDIO_NF_MASKED` is set. The runtime now
also exposes `MODELRT_MAT_ALPHA_MASK`, so a renderer can actually discard those
transparent texels instead of drawing the colored rectangle around a leaf,
fence or sprite-like plane.

`tests/goldsrc_masked_texture.mdl` is a tiny generated regression fixture with a
2x2 masked paletted texture. `make goldsrc-transparency-regression` verifies:

1. the texture info advertises `MODELRT_MAT_ALPHA_MASK`;
2. the material visitor advertises the same render semantic;
3. palette index 255 decodes to alpha 0 while ordinary indices stay alpha 255.

The runtime itself does not call OpenGL and does not choose a blend function.
That remains the renderer/viewer's responsibility.
