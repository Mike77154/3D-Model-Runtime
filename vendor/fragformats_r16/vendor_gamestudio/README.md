# GameStudio vendors — ARG/X/DBO ecosystem, R6

This is an **independently written C89** parser based on publicly documented
GameStudio MDL structures, a locally inspected fragMOTION SDK header, and
static inspection of `FileTypeMDL_GS.dll` in Ghidra. The original DLL, SDK,
original samples, and third-party assets are not redistributed here.

## Generation registry

- `a3/mdl2_probe.c`: MDL2 identification only; **no model decoding**.
- `a4/mdl3.c`: MDL3, 8-bit packed vertex animation frames.
- `a5/mdl45.c`: MDL4 (8/16-bit frames) and MDL5 (per-skin dimensions/mipmaps).
- `a6_a8/mdl7.c`: MDL7 group-based static geometry with select embedded skin
  types and material-only skins.
- `gs_dispatch.c`: dispatches on the magic bytes **not the `.mdl` filename**.
- `mdl_core.c`: common bounded, endian-aware parser and OBJ emitter.

## Implemented

- Legacy MDL3/4/5 header (84 bytes), skins types 0/2/3 (plus 4/5 on MDL5),
  legacy UV index list, corner-indexed faces, one selectable frame of packed
  vertex coordinates, conversion to signed Q16.16, and OBJ output.
- MDL5 mipmap sizes and optional 8/16-bit frame vertex packing.
- MDL7 header with self-describing struct lengths; bone record skipping;
  multiple mesh groups, skinpoint UVs, triangles, geometry, frame metadata
  counts (frame data is skipped, not evaluated); embedded 8/16/24/32-bit
  textures are currently **skipped**, not decoded to images; material-only
  skins are also recognized and skipped.
- MDL7 group vtx normals are not emitted; downstream normal generation
  is required. MDL3/4/5 normal indices similarly await the 162-vector table.
- Bounded triangle table; rejects malformed indices, frame indices, oversized
  counts, invalid texture encodings, and truncated payloads.

## Known limitations

- No A3 MDL2 decoder. The actual mapping to A3 varies by historical version.
- MDL7 skeletal deformers (`deformers != 0`), external skin paths, DDS,
  material animation, frame pose evaluation, texture extraction and LOD parsing
  remain unsupported. Some MDL7 revisions could differ from the tested layout.
- MDL3/4/5 exports use the **first frame by default**. `--frame=N` chooses a
  particular packed vertex frame; not interpolation, skinning, or clip playback.
- Legacy UVs are normalized against the first skin dimensions; the same
  geometry is not remapped for MDL5 skin swaps of different sizes. UVs without
  skin dimensions cannot be normalized and are written as zero. V-axis
  orientation and texture image conversion need independent confirmation.
- C89 static output saturates large coordinates to the representable Q16.16
  range; extreme model dimensions may lose precision.
- The C89 reader has passed **synthetic** fixtures but not a full standalone
  execution against authentic MDL3/5/7 assets. Separately, authentic MDL5 and
  MDL7 buffers from Assimp were inspected through GitHub and structurally
  checked, which informed no-UV/material-only support.

## CLI

```
./fraginspect character.mdl character.obj character.tsv
./fraginspect character.mdl walking_frame.obj walking_frame.tsv --frame=1
./fraginspect character.mdl
```

The tool prints `UNSUPPORTED_OR_INVALID` when the requested variant cannot be
safely decoded. Return code zero indicates success, one means decode failure.
`MDL7` with zero groups reports a structurally valid header but no geometry.
