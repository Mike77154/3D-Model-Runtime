# hl2modelib 1.0.11 — SDK UV/material audit (T-103 coat)

## Verified against the user-provided Source SDK 2013

- `src/public/optimize.h`: optimized VTX `Vertex_t.origMeshVertID` indexes the mesh's vertex; `StripGroupHeader_t::pIndex` indexes its local Vertex_t table, not directly the VVD.
- `src/public/studio.h`: `mstudio_modelvertexdata_t::GetGlobalVertexIndex` and `GetGlobalTangentIndex` use `mstudiomodel_t.vertexindex / sizeof(mstudiovertex_t)` and `tangentsindex / sizeof(Vector4D)`. They are independent offsets. Previous `hl2modelib` used sums of prior model vertex counts for both; fixed in `hl2m_mesher_impl89.inc`.
- `src/materialsystem/stdshaders/vertexlitgeneric_dx9.cpp` and helpers: the shader samples `$basetexture` using surface UVs; a per-vertex RGB preview cannot preserve texel-level cloth patterns.

## T-103 phase1 corpus findings

- 4 material groups; `tyrant_wear` uses 2,757 triangles, a genuine 512 x 512 VTF diffuse atlas, and maps the coat using VVD texture coordinates.
- Single MDL model, global VVD vertex offset 0, no VVD LOD fixups. Thus the *model-offset* correction does **not** change this particular T-103 phase1 model's UVs.
- The real previous problem in the generated preview was sampling color at vertices and interpolating RGB (per-vertex/Gouraud), blurring most of the 512 x 512 atlas detail.
- VTF decoded in top-left row order. VVD/Source UV V must **not** be flipped in the VTF software sampler. OBJ exporter intentionally flips V *only when writing OBJ*, because OBJ convention differs.
- The wear UV island overlay follows the shapes in the original atlas; no evidence was found for a wholesale coat UV remap in this model. Slight UVs outside [0, 1] require texture repeat instead of clamping everywhere.

## Changes

- Add `vendors/source/texturizer/hl2m_uv_filter89.c` using C89 fixed-point texel-center bilinear filtering, with repeat/clamp and negative UV handling.
- `hl2m_master_uv_sample_linear89()` is the new direct sampler.
- `hl2m_master_smooth_fragment_linear89()` interpolates UV/normals/tangents, then samples diffuse/bump/detail maps per fragment and prepares shader parameters. The original nearest-neighbour function remains for compatibility.
- Independent `tests/test_uv111.c` tests texel centers, top-left V, repeat, clamp, bilinear center and the smooth fragment bridge.
- Zero float/double, malloc/free, 64-bit integer operations in C library source. Python rendering is independent and is not linked.

## Presenter integration

1. Decode MDL+VVD+VTX via master; load VMT/VTF and bind textures by material draw group.
2. Rasterize triangles with depth testing and **perspective-correct screen-space barycentric weights** (sum 65536 Q16.16). Compute per pixel, not one sample at each vertex.
3. Call `hl2m_master_smooth_fragment_linear89()` with original unflipped Source UVs; then `hl2m_master_shade89()` with current lights.
4. Respect UV seams (vertices are already split by VVD mesh), VMT `$nocull`, `$alphatest`, and material draws.
5. GPU presenter alternative: use native bilinear texture filtering and interpolate UV in the vertex/pixel shader; the C89 sampler is a CPU reference.

## Remaining discrepancies versus Source 2013

- This sampler addresses only RGBA8 after VTF decoding, for one chosen mip. No trilinear mip filtering, anisotropic filtering, sRGB gamma parity or material-proxy animated transforms.
- It does not evaluate `$basetexturetransform` from VMT or Source's texture transform matrices. This T-103 phase1 corpus's four model VMTs do not request such transforms.
- No actual GPU Source shader compilation/rendering was performed. Python comparison uses an independent **orthographic** software renderer; claims are specifically about recovered texture detail and UV assignment, not complete Source DX9 pixel parity.
