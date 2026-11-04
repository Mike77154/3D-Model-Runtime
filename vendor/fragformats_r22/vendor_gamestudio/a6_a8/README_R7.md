# GameStudio MDL7 Rig Extension (R7)

`mdl7_rig.c` handles three distinct packed-record classes independently of
`mdl_core.c`:

- `gs7_bones`: MD7_BONE fixed 36-byte record, parent index and bind position.
- `gs7_frames`: MD7_FRAME name/counts, MD7_FRAMEVERTEX indices and
  MD7_BONETRANS bone references, with variable strides read from MD7_HEADER.
- `gs7_deformers`: MD7_DEFORMER (16 bytes), MD7_DEFORMERELEMENT (28 bytes),
  MD7_DEFORMERWEIGHT (8 bytes); bounds and influence counts.

Outputs are *metadata only*, not animation playback or fully converted skins.
No floating-point computations, no dynamic allocations. Real-world validation
is still needed for relative ordering and optional layout revisions.
