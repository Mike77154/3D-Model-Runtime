# Phase 04 changes

- Added 10 independent per-generation modules (A4–A8, deformers + weights).
- Added 8 independent binary-format adapters (MDL3, MDL4, MDL5, MDL7, deformation + weights).
- Added shared weight storage/normalization, weighted pose blending, format dispatch and vendor dispatch.
- Implemented conservative MDL7 deformer record traversal and explicit influence parsing (experimental on real files).
- MDL3/4/5 frame deformation advertised with implicit frame weight, not nonexistent skeletal weights.
- Fixed critical weight-table reset across repeated file opens, memory opens and callback opens.
- Updated animation OBJ export to use the new deformer route.
- Expanded A6 compatibility profile to permit MDL7, based on fragMOTION format listing.
- Added synthetic two-bone/50%-50% fixture and security tests for malformed weights.
- Tests: GCC C89, Clang C89, Clang ASan/UBSan; 6,231 truncated/mutated inputs, export (4.5,4.5,5).
- Open questions: actual MED deformer record placement, normal handling, weight conventions, varied MDL7 subversions.
