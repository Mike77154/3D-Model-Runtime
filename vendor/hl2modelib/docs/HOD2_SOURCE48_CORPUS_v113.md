# hl2modelib 1.0.13 — House of the Dead 2 / Source 48 corpus verification

**Source compatibility:** 25/25 models with MDL+VVD+DX90.VTX and
25/25 DX80 variants. All Source MDL v48; VVD v4; VTX v7.

- **Geometry:** 17,412 triangles across 358 draw groups, no invalid triangles
  or warnings from the C89 exporter. 12,740 source VVD vertices in aggregate.
- **Materials:** 321/321 `VertexLitGeneric` VMT parsed; each `$basetexture`
  maps to a VTF within the same addon. 83 VMT set `$alphatest`.
- **Textures:** 321/321 VTF fully decoded, all DXT5.
- **Animation metadata:** Each MDL has one animation and one sequence.
  The 21 creature models use `@ragdoll`; four weapons use `@idle`.
  All have 101 frames, but sampled OBJ geometry at frames 0/50/100 is
  exactly identical for all 25. These are NOT independently playable
  walking/attack sequences from the original arcade game.
- **Visual verification:** Eight models were rendered in offscreen VTK using
  OBJ from hl2modelib and RGBA PNG from hl2modelib's VTF decoder.
  VTK preview illumination is approximate; do not confuse it with Valve
  shader parity, or use it to infer animation support.

## What changed in v1.0.13

- The **core decoding library is intentionally unchanged from the v1.0.12
  Licker/SMD experimental branch**: this corpus exposed no decoding failure.
- Fixed the old `o t103_native` placeholder in both OBJ exporters. They now
  write the actual sanitized Source model name and current codec version.
- Added a strict C89 model/animation metadata CLI:
  `hl2m_corpus_metadata113`.
- Added a portable batch test script: `tools/source_corpus_batch113.py`.
  It composes the actual C89 codec executables (not a substitute parser).
- Retained all existing format vendors, master API and previous tests.
- Strict compilation checked using `-std=c89 -O2 -Wall -Wextra -Werror -pedantic`.

## Build and test

```sh
make clean
make CFLAGS='-std=c89 -O2 -Wall -Wextra -Werror -pedantic -Iinclude' check
make CFLAGS='-std=c89 -O2 -Wall -Wextra -Werror -pedantic -Iinclude' \
     hl2m_export_obj89 hl2m_export_animated_obj89 hl2m_corpus_metadata113 \
     hl2m_probe hl2m_vtf_to_tga89 hl2m_vtf_info_probe112
python3 tools/source_corpus_batch113.py /path/to/unpacked/Hod2Zombiepack --full
```

Run the `source_corpus_batch113.py` script on a directory containing the
nested addons. It does not require original corpus bytes inside the source
release. The optional 3D previews use VTK/Pillow outside the C89 library.

To load a model from your Win32 viewer, use `hl2m_master_decode89` for MDL,
VVD and VTX; build the mesh with `hl2m_master_mesh89`, apply the VMT and VTF
from the addon `materials/models/mr.l/hod2/kankan/` and sample UVs per
fragment. The 1.0.12 Licker/SMD experimental fixes are retained.

**Important:** these are Source workshop conversions of HOD2 assets, not
proof that the original The House of the Dead 2 engine uses Valve MDL.

See full batch results in `HOD2_BATCH_FULL_RESULTS.json` (diagnostic ZIP).
