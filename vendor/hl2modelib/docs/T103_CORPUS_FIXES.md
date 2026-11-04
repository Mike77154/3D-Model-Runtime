# hl2modelib 1.0.2: T-103 real corpus pass

This patch was developed against a supplied Source Engine T-103/Mr. X corpus, using the supplied `hl2sdk-sdk2013`, `source-sdk-2013-master`, and `ValveResourceFormat` sources as reference material. It **does not embed the model/texture assets** in the library source distribution.

## Source format repairs

- Studio MDL version **48** (phase1, phase2, phase3, cloth) and **49** (`tyrant01`) are decoded. The accepted version range is 44–49; actual full-mesh regression coverage in this package is 48 and 49.
- Source `studiohdr_t` body part count/index: byte **232/236**, not 228/232. Retains narrow compatibility fallback for preexisting synthetic output.
- Studio header metadata fields `surfacepropindex`, `keyvalueindex`, `keyvaluesize`, `mass`, `contents`, `szanimblocknameindex`, `numanimblocks` and `animblockindex` are corrected against SDK layouts.
- Source VVD FourCC is `IDSV` (`0x56534449` little-endian); retains a legacy synthetic `SVDI` decoder fallback. Newly written files use SDK-correct `IDSV`.
- A subset of archive files lost precisely the **first byte**: `.mdl` `DST...`, `.vvd` `DSV`, `.dx90.vtx` initial 7, `.vtf` `TF`, and some `.vmt` opening quotation marks. `hl2m_repair_missing_first_byte` repairs only recognizable signatures **in a caller-owned buffer with spare capacity**. Call before regular decode.
- Some corpus `.vmt` files had an extra orphan token outside the final closing brace. The KeyValues parser tolerates that, but still rejects unfinished property values within an open block.
- `hl2m_fixed_to_string` previously overflowed 32-bit integer arithmetic when multiplying the 16.16 fraction by 100,000 and emitted illegal decimal characters. A digit-by-digit base-65536 conversion fixes it, without heap, floating point or 64-bit arithmetic. This is necessary for valid OBJ output.

## Texture support: native C89

- New native library unit `src/hl2m_vtf89.c` and public APIs:
  - `hl2m_vtf_read_info(data, size, &info)`
  - `hl2m_vtf_decode_rgba(data, size, &info, caller_rgba_buffer, capacity)`
- VTF versions **7.0–7.5**, compressed **BC1/DXT1 (format 13)** and **BC3/DXT5 (format 15)**; mip chain and 7.3+ high-resolution resource offset lookup.
- RGBA8 output. Caller owns input/output memory. No `malloc`, floats, 64-bit math or third-party codec dependency.
- Single frame/slice only for now: cubemaps and volume textures explicitly return unsupported. Other VTF pixel formats do not falsely decode as BC1/BC3.
- `examples/hl2m_vtf_to_tga89.c` converts to top-origin uncompressed 32-bit TGA for offline inspection. Limit is 2048 x 2048 **in that example**, not the decoder API.
- Native decoded TGA checked pixel-for-pixel against a Pillow DDS/BCn decode on examples with BC1, BC3, mipmaps, header repair and resource offsets: all six samples matched exactly.

## Real corpus topology / textures

| Corpus model | MDL | VVD vertices | Renderable triangles | Draw materials | Bones |
|---|---:|---:|---:|---:|---:|
| TyrantPhase1 | 48 | 4,854 | 7,557 | 4 | 57 |
| TyrantPhase2 | 48 | 7,506 | 11,582 | 1 | 37 |
| TyrantPhase3 | 48 | 15,212 | 21,610 | 1 | 41 |
| TyrantCloth_Down | 48 | 364 | 630 | 1 | 27 |
| Tyrant01 | 49 | 4,614 | 7,513 | 5 | 81 |
| Tyrant01_arms3 | 49 | 1,428 | 2,146 | 2 | 43 |
| Tyrant01_arms_fix | 49 | 1,428 | 2,146 | 2 | 43 |

Full geometry assembled from **native `.mdl` + `.vvd` + `.dx90.vtx`** using `hl2m_build_engine_model`, followed by `hl2m_engine_model_validate`. Texture names resolved per material. 20/20 VMT files parsed after archive-header recovery. Two `arms_ani` MDLs could not be passed through the mesh exporter because the supplied corpus has no matching VTX/VVD pair for those files.

## C89 build / smoke test

```sh
make clean
make -j2 CFLAGS='-std=c89 -Wall -Wextra -Werror -pedantic -Iinclude'
make check CFLAGS='-std=c89 -Wall -Wextra -Werror -pedantic -Iinclude'
```

All existing synthetic model/runtimes tests and the new `test_vtf89` should pass. Use a native model bundle:

```sh
./hl2m_export_obj89 "path/to/TyrantPhase1.mdl" \
    "path/to/TyrantPhase1.vvd" \
    "path/to/TyrantPhase1.dx90.vtx" \
    "TyrantPhase1.obj"
./hl2m_vtf_to_tga89 "path/to/tyrant_head.vtf" "tyrant_head.tga"
```

The OBJ exporter writes `.obj`/`.mtl` with correct normals/UVs; `map_Kd` points to matching `.png` filenames. Use a TGA/PNG conversion utility outside C89 or supply the original textures to an engine with native VTF support. The test preview bundle contains fully resolved PNG files, and 3D images rendered **off-line with VTK** (VTK is **not** a dependency of the library and is not embedded into the engine).

## Explicit non-goals / limitations of this pass

- This proves **static bind-pose geometry** and texture mapping, not every animation or IK/physics behavior from the actual game. The existing synthetic sequence runtime regression passes, but native Tyrant animations need a follow-up corpus-driven test.
- The `.phy` files in some parts of the supplied archive may also be shifted by one byte. PHY lacks a trustworthy four-byte signature, so the repair routine intentionally **does not guess**; dedicated validation is still needed.
- Source materials include additional shader effects (`$bumpmap`, `$nocull`, specular/alpha/shader semantics). The offline preview focuses on diffuse texture appearance. The C89 library exports the material names and supports raw RGBA decoding; rendering shader effects remains the consuming engine's responsibility.
- Because some ports use different axis conventions, the preview camera adjusts automatically; the codec does not forcibly rewrite the model's coordinate system.
- Supplied third-party proprietary meshes/textures and SDK code are not vendored into the codec source archive. Previews and export artifacts are distributed separately as user-supplied corpus derivatives.
