# Quake II MD2 + PCX corpus audit — Runtime 75 / C89

## Result

- **65/65 MD2 opened and visited** through ModelRuntime provider **45**.
- **11,714 authored vertex frames** visited through `modelrt_visit_animation_frame` without inventing FPS.
- The original runtime opened 49/65; **16** MD2s failed on a single trailing zero byte.
- The new decoder accepts **at most 16 trailing zero bytes**, rejects nonzero trailing data, truncation and extra-long zero padding.
- **243/243 PCX** decode through the public runtime image API (new `MODELRT_IMAGE_PCX` bit `0x80`).
- **241/243 PCX** reproduced RGBA bytes exactly against an independent reference decoder.
- Two Quake II icon textures (El Nakedo: `DoubleD_i.pcx`, `mrfantastic_i.pcx`) decode with PCX89, but the independent decoder reports truncation/irregular encoding. Treat them as *not independently validated*, not proof of perfect source art.
- Some player body models (`tris.md2`) contain zero embedded skin references: skins are separate PCX files and must be selected/associated by the caller. This update does **not** autoassign arbitrary skins.
- `UnfinishedPlayableAndOpenSourceStuff.7z` contains a `pak3.pak` with **32 PNG and 2 PCX**, not MD2 models. `Player-Incarnate` and `Wayland` are skins-only packages.

## Provenance and counts

| Archive | MD2 | PCX |
|---|---:|---:|
| Player Incarnate RAR | 0 | 50 |
| Yohko ZIP | 2 | 7 |
| Wayland ZIP | 0 | 2 |
| Tourettes Guy ZIP | 21 | 13 |
| Pepsi Man ZIP | 14 | 12 |
| El Nakedo ZIP | 14 | 137 |
| Uberbitch ZIP | 14 | 20 |
| Unfinished 7z (nested PAK) | 0 | 2 |
| **Total** | **65** | **243** |

## Changes

1. `vendor/gackt_3dcodecs/md2fx_c89/src/md2_decode.c`: strict zero-only padding tolerance for MD2 `ofs_end` mismatch.
2. `vendor/pcx89/src/pcx89.c`: no-heap, no-float PCX RLE decoder, one-plane indexed or three-plane RGB, palette check for indexed mode.
3. `src/image/model_image89.c`: PCX probe before loose TGA probes, support RGBA decoding.
4. `include/model_runtime89.h`: `MODELRT_IMAGE_PCX`, expanded `MODELRT_IMAGE_ALL`.
5. `Makefile`: `IMAGE_PCX=1` default; opt out using `make IMAGE_PCX=0` from clean build.
6. `tests/quake2_md2_corpus89.c` and `tests/quake2_pcx_corpus89.c`: C89, caller-supplied asset files; **no copyrighted samples bundled**.

## Build/test

```sh
make -j4 libmodel_runtime89.a
make quake2-md2-corpus-build quake2-pcx-corpus-build
./tests/quake2_md2_corpus89 /path/to/model1.md2 /path/to/model2.md2
./tests/quake2_pcx_corpus89 /path/to/skin1.pcx /path/to/skin2.pcx
```

With this corpus all MD2 opening/geometry/UV/frame sampling tests passed. Q2 normals were checked for valid native index range in corpus, **not independently checked for rendered shading**. **Visual QA, automatic skin association and MinGW32 build remain unverified**. The general image-codec regression suite could not run because its fixture files are missing from the source-only package. Existing RE4 PMD/FCV synthetic, HOD2 Phase12 table and Gun Survivor synthetic regression targets were run successfully.

## Report files

- `docs/QUAKE2_MD2_CORPUS65_RESULTS.csv` — per-file model and animation validation, with source SHA256 but no assets.
- `docs/QUAKE2_PCX_CORPUS243_RESULTS.csv` — per-file image validation.

Only source, tests and metadata are included, not models, image assets, PAKs or audio files from the user-provided corpus.
