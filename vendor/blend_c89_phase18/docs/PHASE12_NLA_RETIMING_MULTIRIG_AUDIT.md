# Phase 12 — Conservative NLA retiming and multi-rig timeline audit

## Objective and honest scope

This builds on **Phase 11**, not a rewrite. It improves the **single-contributing-strip** NLA path and adds a **read-only per-armature Action timeline report**. It does **not** implement NLA blending, nor does the OBJ evaluator accept several rigs at once. The latter needs per-object mesh/armature isolation, independent bind matrices and deformation palettes.

## Changes made

- `src/bc_anim_bindings.c`: new bounded `bind_nla_timed` traversal across many NLA tracks/strips, selecting a clip only if **exactly one unmuted, enabled strip contributes at the given scene frame**.
- Skips muted/disabled tracks and muted strips; ignores **UI-only** active/selected/protected flags where safe. Unsupported global NLA solo/edit/disable flags reject instead of being guessed.
- For a clip with saved `start`, `end`, `actstart`, `actend`, `scale`, `repeat` and unit influence, supports positive scale (including fractional) and **integer repeats 1–16**, with checked Q16.16 math. Both span duration and total strip duration must agree exactly in fixed-point units. Mismatched stored parameters reject rather than rescale silently.
- For times inside a strip: `cycle = (actend-actstart)*scale`; `local = actstart + ((scene-start) mod cycle) / scale`. At **the final strip endpoint**, local time equals `actend`, not `actstart`. Any overflow returns unsupported.
- `--play-obj ... auto` and `--play-sequence ... auto` use this mapping through the existing Phase 11 entrypoint. Inactive strips in other tracks no longer cause automatic rejection.
- New `--rig-timeline FILE.blend FRAME OUT.tsv`: one row per detected armature Object, with the associated Action ID, slot, and mapped Action-local frame. Rows may show `none`, `direct`, `nla`, `unsupported_nla`, `unsupported_mixed`, `unsupported_slot`. This **audits** multi-rig scenes but does **not deform** multiple rigs.
- `tests/test_phase12_timing.py`: end-to-end synthetic rig fixtures in 32/64-bit pointers and both byte orders, including frame retiming, integer repeats, muted tracks/strips, multiple tracks, overlap refusal, UI flags, unsupported global flags, and a two-rig audit.

## Guaranteed explicit limitations

- This is **not Blender's NLA mixer**. Multiple contributing tracks/strips, additive/subtractive/combine modes, transitions, extrapolation outside clip range, partial repeats, reverse playback, strip FCurves/modifiers, and non-unit influence are rejected.
- Only stored active Action or a single contributing NLA clip can be played per armature. A simultaneous active Action and NLA tracks is still rejected.
- The **single-rig LBS OBJ exporter** remains limited by constraints, IK, parenting/dependency graphs, B-Bones, drivers, multiple armatures, material/shape-key evaluation, and mesh ownership issues documented in prior phases.
- `--rig-timeline` reports serialised bindings, **not a visual/physics validation** of all Blender rigs. Action IDs are decoder enumeration indices, not stable names.
- Q16.16 represents fractional frames only to 16-bit fractional precision and can reject legitimate huge time ranges via checked overflow. Repeats capped at 16 to bound loop complexity.
- Linux compression remains linked to static **zlib and Zstandard libraries** distributed in the vendor directory, which may use heaps internally and are **not** all-C89. The project-authored C sources remain C89 with no explicit heap allocation and no floating-point types. Win32 compressed-file support remains unverified.
- The seven previously tested **authentic** Blender assets exercise **static geometry extraction**. The dynamic NLA rig tests in this phase use **synthetic SDNA fixtures**, not genuine authored and Blender-verified animated assets.

## Commands

```sh
make clean all
./blendc89 --play-obj synthetic_rig.blend 9 frame9.obj auto
./blendc89 --play-sequence synthetic_rig.blend 1 23 frames auto
./blendc89 --rig-timeline multirig.blend 9 timeline.tsv
python3 tests/test_phase12_timing.py
python3 tests/test_phase11_auto_play.py
python3 tests/test_phase07_real_corpus.py /mnt/data/_blend_real_corpus
```

## Cross-version source research

Structure and flag meanings checked against uploaded Blender **3.6.3** `DNA_anim_types.h` (NLA strip/track flags and clip parameters) and **4.5.14** `DNA_anim_types.h` (slot linkage and new NLA schema). These Blender source files were used solely as references; **no GPL source was copied into the project's C implementation**.

## Next necessary step

1. Add a truly per-object/bone skinning pipeline to avoid mixing separate rigs and meshes.
2. Obtain genuine `.blend` rig with known timeline (one clip with scale/repeat first) and compare vertex samples frame-by-frame against Blender's own evaluated output.
3. Implement precise NLA blending only after reference-based numerical tests, including rotation/scale blend semantics.
