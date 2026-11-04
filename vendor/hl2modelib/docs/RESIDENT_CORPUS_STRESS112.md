# Resident NPC + Chronicles: HL2 model codec v1.0.12 stress test

## Input archives (provided by user)

- `massive resident NPC hl2.zip`: 83 Source MDL models (81 MDL v48, 2 MDL v44), 74 with complete MDL/VVD/VTX mesh sets.
- `chronicles package hl2.zip`: 17 Source MDL v49 models, all with complete mesh sets.
- Combined: 100 MDL models, **91 complete mesh sets**, 9 MDL metadata/animation banks lacking complete mesh companions.

## Measured outcomes

- 100/100 MDL metadata parsed. 91/91 complete mesh sets successfully assembled via portable C89 codec. No mesher warnings or triangles skipped in these calls. This is geometry assembly, NOT a proof of original runtime visual or animation parity.
- 438/438 VMT shader declarations parsed. Shader classes include VertexLitGeneric, UnlitGeneric, LightmappedGeneric, Eyes, UnlitTwoTexture. Parsing a material does not prove all GPU shader paths.
- 554/554 VTF headers parsed. The corpus contains DXT1 (178), DXT5 (155), BGRA8888 (201), ABGR8888 (11), BGR888 (9); six representative VTF image files covering all five formats successfully decoded to RGBA/TGA. NOT all 554 images were individually decoded or compared against Valve's renderer.
- The MDL metadata inventory declares 1,195 raw animations and 977 sequences. These were NOT played, skinned or crosschecked exhaustively; animation fidelity remains pending.
- Six diverse 3D assets were exported to OBJ/MTL and displayed as untextured geometry: Nemesis, RE4 Regenerator, RE:ORC USS Zombie, T-002, Lisa Trevor, Leech Queen. The picture is diagnostic clay render, not Source-shader output.

## Actual incompatibilities detected and corrected

1. Signed negative relative offsets in VTX replacement tables: the old decoder rejected valid backwards relative references. These are now bounded and validated correctly.
2. Both classic 27-byte strip-header layout and the observed 35-byte variant can now be handled by validating prospective layouts for multistrip groups.
3. Too-small fixed array capacities rejected large, valid models. Increased compile-time maxima for VVD, VTX indices and output engine draw vertices; no heap allocation.
4. VTX compiler put replacement strings between fixed-size table headers: adjusted writer to emit contiguous headers before string data. Preserves v1.0.11 synthetic roundtrip parity.
5. VVD fixup records were read but not actually applied to the position and tangent arrays. They are now repacked in Source LOD order. This corrected stretched spikes observed in Nemesis OBJ output.
6. Extended pure C89 VTF decoding to BGRA8888, ABGR8888 and BGR888 alongside original DXT1/DXT5.

## Reproduce (GNU Make + Python3 test driver)

```sh
make clean
make check CFLAGS='-std=c89 -Wall -Wextra -Werror -pedantic -Iinclude'
make hl2m_bundle_probe CFLAGS='-std=c89 -Wall -Wextra -Werror -pedantic -Iinclude'
python3 tools/validate_corpus112.py 'massive resident NPC hl2.zip' 'chronicles package hl2.zip' --probe ./hl2m_bundle_probe --json corpus112_results.json
```

The two supplied ZIPs are **not included** in the FULL_SOURCE release to avoid repacking the large corpus; provide them separately to rerun real-corpus validations. C89 regressions are in `tests/test_corpus112.c`, `tests/test_vvd_fixup112.c`, `tests/test_vtf_uncompressed112.c`.

**ABI note:** compile-time capacities increased, changing public struct sizes. `HL2M_MASTER_ABI89=0x0112`. Recompile consumer modules, although previous entry points were preserved. The portable codec adds no float/double, int64 or malloc/free.

## Open work

- Per-pixel normal mapped full mesh previews for this corpus, true Source material/shader parity, and full rendering of all 91 models.
- Validate facial channels, interpolation and sequence blending for each model rather than using metadata counts as animation pass counts.
- Independent reference SDK comparison for each extended VTX layout and for animation-only v44 models.
