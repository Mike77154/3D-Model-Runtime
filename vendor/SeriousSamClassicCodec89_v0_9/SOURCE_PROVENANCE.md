# Provenance / reproducibility

All distributable C files in `src/` and headers in `include/` were newly written for this project. Format data offsets and chunk layouts were independently implemented from inspection of the public Serious Engine 1 serialization routines. This archive does not bundle the original Croteam source code or proprietary artwork.

The user supplied the archives `SeriousSamClassic-main.zip` and `Serious-Engine-master.zip` as source references. The archive `SE1_10.gro` embedded in Serious-Engine-master was used only for automated local validation; it is NOT included in the deliverable.

Validation on 2026-10-08:
- 390 / 390 original MDAT models parsed: V010=370, V009=16, V008=2, V007=2.
- 479 / 479 nonzero secondary MDAT MIPs selected successfully.
- 724 / 724 TEX inputs inspected: 690 raw TVER4, 16 raw TVER3, 18 effect/alternate texture metadata only.
- 2 / 2 original MESH11 assets parsed.
- 3 / 3 original ANI ADAT resources parsed.
- 3 / 3 SMC text scripts previewed.
- 69 / 69 selected GRO entries extracted byte-identically compared to Python zipfile.
- Synthetic regressions: MDAT V002–V010, MESH11 and 12, SKEL6, ANIM14, ADAT, TEX3/4, GRO stored/deflate, and direct mesh getter API.

License note: The Croteam reference engine is GPL-2.0; downstream users should not treat that license as covering their own proprietary assets or third-party code. The new codec does not contain verbatim engine source code.
