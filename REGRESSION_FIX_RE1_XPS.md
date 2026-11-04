# RE1 dispatch + XPS external texture regression fix

## RE1 / shared .EMD routing

RE1codec v0.3-mvref itself was not replaced or modified by the advanced image-decoder pass.
The risky layer was provider dispatch: `.emd` is shared by RE1 and RE3, but the runtime previously
selected one best probe and stopped if that provider later rejected the file.

Fixes:
- RE3 probe validates the complete 15-entry section table and mandatory body MD2 header before returning YES.
- `modelrt_open_file()` keeps all positive candidates and falls through on FORMAT/CODEC/UNSUPPORTED.
- RE1 open/validate failures now retain the native `re1c_result_string()` detail instead of only "bad format".
- RE1 Bank A unbound policy is unchanged.

## XPS external texture orientation

The XPS provider UV behavior was still correct:
- texture `uvLayerId` selects the UV layer;
- V is converted as `1 - V`;
- UV coordinates are not clamped, preserving tiling.

The regression was in the new external-image path. WIC historically flipped decoded top-left rows before
classic OpenGL upload. `model_image89` is backend-neutral and returns top-left RGBA, so bypassing WIC caused
an origin mismatch even though UVs were correct.

Fixes:
- External textures no longer take the generic `modelrt_texture_decode_rgba()` shortcut in the viewer.
- Viewer resolves the real file path first (preserving manual overrides and trace diagnostics).
- Runtime image89 decodes the file.
- Viewer flips RGBA rows exactly once before `glTexImage2D`.
- WIC fallback is unchanged; it already performs the equivalent flip.
