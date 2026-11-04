# R11 GameStudio MDL7 — diagnostic pivot skinning, not native pose playback

## Independent C89 implementation

New `--skin-pivot` computes the MDL7 **3x4 column-storage** interpretation
used by Assimp's `MDLLoader.cpp`, converting its 12 matrix values into the
runtime's row-vector Q16.16 affine layout. All C code uses static arrays;
there are no `float`, `double`, `long long`, dynamic allocation, or heaps.

It then computes each influenced point as:

```
p_bone = bind_pos + transform( matrix_frame[bone], p_base - bind_pos )
p_final = sum_bones( weight_bone * p_bone )
```

Where `bind_pos` comes from the MDL7 absolute bone XYZ field and the `matrix_frame`
comes from the *selected* `MD7_BONETRANS` record. Missing bone matrix = identity.
Weights of influenced vertices must total Q16.16 unity within 3 ulps.

### LIMITATIONS

This is **a controlled hypothesis**, not confirmed GameStudio skinning.
The interpretation assumes the frame matrices are already *rest-relative* and
does **not** propagate parents through the pose hierarchy. It is possible that
an actual A6/A7/A8 animation requires additional inverse bind and parent-space
transforms. Other unhandled MDL7 variants remain unsupported.

The earlier `--skin-raw` R10 mode is preserved unchanged for A/B comparison.
Normal OBJ geometry (without skin flags) is likewise unchanged.

## Fresh reverse-engineering results

Real Ghidra 12.0.4 `-noanalysis` targeted disassembly against the supplied
fragMOTION `FileTypeMDL_GS.dll`: SHA256
`449b57d63da7bd7f5ce8acf20116aea07cb8744b242ddf20b668b51d770ac057`.

- `0x1000c350`: 206 instructions, dispatcher candidate.
- `0x1000c600`: 1,122 instructions; 6 x87 opcodes in the explicit trace.
- `0x10010890`: 3,937 instructions; 89 x87 opcodes in the explicit trace.
- `0x1001d9e0`: 15 instructions, x87 `FSTCW/FLDCW/FISTP` integer conversion helper.
- In the MDL7 routine, `0x10010fde..0x10011054` converts four values with
  `FLD; FMUL [0x1002e270]; FADD [0x1002e410]; CALL 0x1001d9e0`, then
  combines them by `SHL 8` and `OR`. Read-only constants at `0x1002e270=255.0`
  and `0x1002e410=0.5`. This confirms an eight-bit four-channel packing path,
  rather than evidence for *matrix* quantization. Specific channel semantics
  require more data-flow tracing.

`mdl7_color.c` implements a separate, testable Q16.16 four-channel packer
mirroring the observed arithmetic for inputs 0..1. **Clamping** of values
outside 0..1 is defensive behavior, not proven to match the original plugin.
It is deliberately not wired into texture parsing until material field usage
is confirmed by a more complete data-flow analysis.

## Tests

`make test` runs R4..R10 regression plus new R11 cases:

- 90-degree Z rotation around bone at (0,1,0), with 1/4 influence on vertex0
  yields `(0.2500,0.2500,0)`; bind Y=2 yields `(0.5000,0.5000,0)`.
- Selected frame 0 reproduces base vertex, and raw vs pivot previews differ.
- Rejects NaN, bone parent cycles, incomplete weights and conflicting CLI flags.
- Four-channel packed Q16.16 values produce expected `0xff0080ff`.

These R11 rig tests are **synthetic**. No native bone-animated commercial MDL7
model has been validated; the four original ARG models are separate regression
corpus for the existing ARG vendor.

## R12 extension

Use `--skin-hierarchy` to **experimentally** propagate rest-relative local
bone matrices through the parent tree before applying weighted deformation.
The R11 `--skin-pivot` stays available as an independent-mode control. The
matrix convention remains hypothetical until corroborated with genuine
skinned GameStudio files; see `research/R12_GHIDRA_FINDINGS.md`.
