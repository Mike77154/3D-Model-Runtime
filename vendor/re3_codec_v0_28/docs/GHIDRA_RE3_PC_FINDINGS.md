# RE3 PC Ghidra/objdump: verified texture polygon rebasing

## Grounding

- Ghidra 12.0.4 in headless no-autoanalysis mode, with exact code locations manually disassembled and decompiled.
- `ResidentEvil3.exe` SHA-256: b6b16582d3fbbd2b05d6e76c26a7925fa1952f2d2157a885a6b6675d9290db36
- `RE3_MERCE.exe` SHA-256: bc0d29d5cbe19dd36b67e2d9cd8104151240709b9503781699c3ffcd5079b472
- RE3 `0x004319F0` / Mercenarios `0x00431A40` are the same 153-byte texture/polygon preparation routine displaced by `+0x50` in the executable.
- This routine first invokes the helper at `0x00431A90` (RE3) / `0x00431AE0` (Mercenarios) (further interpretation pending); then loops mesh parts, triangles (stride 12), quads (stride 16), adding texture atlas offsets into packed polygon words.

Ghidra recovered (simplified mathematical equivalence):

```c
polygon_dword_0 += clut_y * 0x00400000u; /* <<22 */
polygon_dword_1 += page_x * 0x00010000u; /* <<16 */
```

The actual input values are caller-supplied VRAM positioning values; they are not inferred or guessed. Additions are modulo 2^32 and operate on packed little-endian words. Do not apply a second time if polygons are already rebased. Preserve original files; operate on writable copies only.

In RE3's `0x004386B0`, pointer index is read via `piVar10=(int *)(*base+base)` and section pointer fields are written from this table. The function prepares names `emXX.tim`, `emXX.emd` or `emd08/emXX.emd`, loads resources using routine `0x00426540`, and calls the polygon preparation routine at `0x004319F0`.

## Why this matters

This confirms a missing **runtime texture atlas relocation** step, distinct from raw TIM/EMD UV extraction. A TIM preview using the original TIM coordinates can appear correct; a combined VRAM atlas may require these relative page/CLUT shifts before sampling.

The new API is opt-in. It does **not** change default texture sampling or claim that all seams, morphs, damage clip mappings, lighting or source runtime frame rates are solved.

## REA version constraint

REA 6.8.0 verification refuses Ghidra 12.0.4 with `Bridge sources require Ghidra 12.x (>=12.1) (verified with 12.1.4)`. Ghidra 12.0.4 itself can decompile directly. REA runtime was not compiled or invoked in this report.

## Repeatability

`make test_pc_rebase` then `./test_pc_rebase`.
`make re3emd_pc_rebase_audit` then `./re3emd_pc_rebase_audit EM30.EMD`.
The audit processes original EMD record copies and does not modify the source or TIM data. To test full corpus, run the audit once per EMD.

## Binary01 morph runtime: recovered and cross-checked with objdump

NEW: `0x00438200` sets up per-part morph streams from binary01 bitmask, storing each pointer in model-part objects separated by `0xBC` bytes. `0x00438260` starts from **absolute base coordinates** in binary01 frame 0 and reads subsequent signed short xyz deltas. The key x86 loop has the exact form:

```asm
0x004383f7 movsx edi, WORD PTR [ecx]
0x004383fa imul  edi, ebx       ; ebx = 12-bit blend weight
0x004383fd sar   edi, 0xc       ; signed / 4096, round toward -infinity
0x00438400 add   WORD PTR [eax-4], di
```

After each weighted morph delta, the function optionally handles joint animations and transformations. REA's Ghidra bridge was NOT used; this pseudocode was generated directly by Ghidra 12.0.4 and compared to GNU objdump. Equivalent functions in Mercenarios are relocated by +0x50 (`0x00438250`, `0x004382B0`).

New API `re3c_morph_blend_local_vertex`: frame 0 absolute; subsequent frames interpreted as deltas weighted independently by explicit 0..4096 values. Its arithmetic emulates signed SAR12 for negative values. `re3c_animated_body_vertex_morph_blend` additionally uses the existing DAT skinning and joint matrices. This is a **verified arithmetic primitive**, not a reconstruction of gameplay rules or real time-varying weights. It rejects position overflow rather than simulating i16 wrapping; the exact overflow handling remains future work.

The four tracks of Grave Digger can be blended individually; use explicit weights from caller, not inferred state.
