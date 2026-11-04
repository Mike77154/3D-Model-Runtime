# DTS89 v3.8: native Torque UV versus OBJ and raster sampling

This corrects a **visualization/export convention mistake**, not the binary DTS reader's texcoord offsets.

Torque stores Point2F UVs in the mesh, sampled against the image's top-left pixel origin in the reference data path. Our external Python preview renderer receives images decoded in row-major/top-left order. In v3.7 we wrote the DTS V coordinate unchanged into OBJ, then the previewer treated it as OBJ's bottom-origin V and used `row = (1 - V) * (height - 1)`. That produced a vertical mirror of the texture atlas. The facial, shirt and pants maps were sampled in the wrong regions.

In v3.8, `dts89_uv_texturer_get(mesh,layer,index,convention,&uv)` makes UV convention explicit.

- `DTS89_UV_TORQUE_TOP_LEFT`: pass through the native decoded `(U,V)` (use this when sampling pixels in top-first image-row order).
- `DTS89_UV_OBJ_BOTTOM_LEFT`: return `(U, 1-V)` once when exporting OBJ. An OBJ viewer expecting bottom-origin V (or `ty=1-V_obj` in the test renderer) then samples the same top-first image rows as Torque.

**Never flip both exported UVs and textures**. Do not destructively flip raw DTS buffers. Do not clamp or fractional-wrap UVs in the codec: tile and address modes belong to the renderer/material.

The pose37 OBJ exporter now uses OBJ-bottom-origin UV explicitly. All other pose/skinning and geometry math is preserved. No heap or floating point in C89 source. The Python PIL/NumPy screenshot renderer is an external test only and is not distributed as part of the C89 library.

## Corpus

Zombie, Zombie_02, Zombie_03 and Zombie_05 exported without UV conversion errors. MegaZombie mesh23 vertex232 and Zombie_04 mesh7 vertex232 have a source UV value that cannot be expressed in 32-bit signed Q16.16; exporter logs that vertex and substitutes (0,0) **only in the OBJ preview**. The original binary32 bits are retained in the decoded DTS and no structural mutation is done. In the current view those vertices also occur in triangle indices. A higher-range UV number system or source-format check is still required there for fidelity.

**Verified:** native DTS reader -> 6 shapes -> v3.8 OBJ -> MTL -> external software raster preview. NOT verified: pixel-perfect equivalence with Torque3D's GPU pipeline, texture address flags, second UV channel, DDS compression/normal/specular, or all exporters and engine versions.
