# RE3_codec v0.15.0 — Reference-pose rendering audit

## Inputs

- ResidentEvil3.exe / RE3_MERCE.exe from the preceding PC binary audit (not re-run via REA in this revision).
- v0.14.0 recovered C89 source plus full 2026-08-21 RE3 corpus.
- 100 paired EMD/TIM files across `EMD` (69) and `EMD08` (31).

## Concrete fixes

1. Public `re3c_reference_body_vertex()` transforms raw MD2 vertices into reference-pose world space (rigid-joint translations; DAT owner A/B and Q0.7 seam blending). Previously the legacy viewer drew raw per-piece vertex positions with no skeletal assembly.
2. New `re3emd_preview` C89 software rasterizer uses a fixed point orthographic camera, z-buffer, affine UV interpolation and TIM/CLUT/TPAGE texture lookups. No heap, no float, no external dependencies.
3. Quad triangulation uses `0-1-3-2`, as documented in the RE3 model format. Splits into triangles `0,1,3` and `0,3,2`.
4. The default main view suppresses `extra_unbound` and `dynamic_morph` submeshes in **preview only**. They remain intact in EMD and can be rendered with the `all` switch. This prevents the vertical strip artifact obscuring Nemesis' face in EM34. Exact game flags for those parts are still under investigation.
5. Windows batch build updated for both new command-line utilities.

## Verified tests

- gcc -std=c89 -pedantic -Wall -Wextra -Werror -O2: compilation passes.
- Synthetic core and TIM: PASS.
- EMD structural corpus: 100/100 PASS.
- TIM corpus: 100/100 PASS.
- EMD+TIM corner sample: 100/100 PASS.
- Reference-pose vertex audit: 100/100 PASS.
- Software textured rendering: 100/100 PASS, 0 failed TIM sample calls. This does **not** demonstrate pixel-level game equivalence.
- EM34 Nemesis (533 vertices, 639 source faces): new front view shows face/arms/coat/boots; 557 visible faces after removing optional preview submeshes (75 faces excluded); 0 texture lookup failures.
- EM20 Cerberus: limbs assembled in reference pose; textured preview.
- EM65 character in blue outfit: coherent preview with four limbs; identity should be confirmed separately.
- EM30 large worm-like enemy: coherent elongated geometry; camera framing could be optimized.
- EM34 from EMD08 is binary-different from EMD/EM34 but the v0.15 front rendered image is pixel-identical.

## Still not implemented / qualified

- REA/Ghidra was not runnable in the current runtime; no decompiled PC graphics functions were proven here.
- This preview has no PSX GTE/GPU exactness, perspective-correct texture mapping, native normals/light, original draw-order or blend behavior.
- It does not yet sample an animated pose or compute dynamic morph frames.
- Unknown optional geometry should be classified using gameplay script and animated state, not permanently deleted.
- Software renderer is only validated on this supplied 100-pair corpus, not arbitrary malformed files or RE5/RE6 formats.

## Useful external format references

- https://github-wiki-see.page/m/pmandin/reevengi-tools/wiki/.EMD-(Resident-Evil-3) (quad winding and EMD layout)
- https://www.tapatalk.com/groups/residentevil123/re3-npc-to-pc-t1081.html (EM34 Nemesis filename)
