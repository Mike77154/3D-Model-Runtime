# StudioHalfLife Ultra

`StudioHalfLife Ultra` is a third-layer extension over `studiohalflife_extensions_plus`.
It keeps the same rules: C89, no heap allocation, no `float`/`double` runtime math, caller-owned buffers, fixed-point 16.16.

## New modules

```txt
hlfp_studiohalflife_ultra.h/.c
hlfp_softmdl_ultra.h/.c
test_studiohalflife_ultra.c
demo_render_studiohalflife_ultra.c
```

## What Ultra adds

### Runtime/model helpers

- Body value resolver:
  - `body_value -> bodypart local model -> global model index`
- Skin resolver:
  - `skin_family + skinref -> texture index/name`
- Attachment world builder:
  - attachment origin and axis vectors transformed by bone matrices
- Layer stack builder:
  - base sequence + active autolayers
  - fixed-point weight ramping
- Animation driver:
  - frame-position or seconds-based decode
  - optional autolayers
  - optional root motion
  - optional procedural bones
  - optional IK-rule pass
- IK rule evaluator:
  - active weight ramp from start/peak/tail/end
  - chain path through existing two-bone IK lite
  - fallback direct IK target path

### Deep audit

`hlfp_shlu_deep_audit_model()` checks:

- bodypart/model mapping
- mesh skinrefs
- skin family texture mapping
- texture pixel/palette ranges
- attachment bone ranges
- event frame sanity
- external seqgroup buffers
- transition table values
- Xash reused model weight table ranges

### QC/decompiler extras

`hlfp_shlu_write_qc_decompile_ultra()` wraps the Plus QC scaffold and appends:

- `$eyeposition`
- `$bbox`
- `$cbox`
- `$flags`
- `$controller`
- `$attachment`
- event comments
- IK chain/rule comments
- autolayer comments
- keyvalue summary

### Companion manifest

`hlfp_shlu_write_companion_manifest()` writes a practical manifest for:

- main `.mdl` / `.dol`
- `T.mdl` / `T.dol` texture companion
- seqgroup companion files
- body/model/mesh/attachment counts
- extension summary

### Debug renderer

`hlfp_softmdl_ultra` adds a visual debug sheet:

- checker background
- textured software model render
- skeleton overlay
- hitbox AABB overlay
- attachment axis overlay
- TGA output through the existing `hlfp_soft2_write_tga32()`

## Build

```sh
make clean all test_output.txt studiohalflife_ultra_preview.tga
```

Expected line in `test_output.txt`:

```txt
studiohalflife_ultra_ok=1
```

## Notes

This is still not a bit-perfect replacement for StudioMDL/HLMV/Xash renderer behavior. The IK pass remains a lightweight runtime evaluator, but it now has a cleaner chain/rule abstraction and plugs into a real animation driver instead of being just a standalone helper.
