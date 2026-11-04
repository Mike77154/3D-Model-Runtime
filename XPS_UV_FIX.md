# XPS UV-layer / V-origin fix

The XPS codec keeps native UV data unchanged.  The runtime provider now adapts
that native data correctly for the renderer-facing view:

1. The texture selected for a submesh chooses its declared `uv_layer` instead
   of hard-coding UV layer zero.
2. Runtime/viewer V is emitted as `1 - raw_xps_v`, matching the established XPS
   -> OBJ / Blender bridge convention.
3. U/V are never clamped. Values outside 0..1 remain intact so `GL_REPEAT`
   preserves intentional texture tiling.
4. An invalid texture UV-layer index falls back to layer zero for legacy/broken
   assets instead of reading outside the vertex UV array.

The native `xps_vertex.uv[][]` representation in the vendored codec remains
raw and authoritative; only the provider view is transformed.

Regression fixture: `tests/xps_uv_layer_regression.mesh.ascii`.
It deliberately assigns the diffuse texture to UV layer 1 while UV layer 0 is
zeroed, and includes UVs outside 0..1 to verify repeat preservation.
