# RE3 Codec v0.25 — PLW Shared VRAM, Guarded Damage Retarget, Special Cases

## What changed

1. **Player+weapon VRAM composition (new)**: `re3p_compose_vram(out, pld_tim, plw_tim)` overlays only the advertised TIM image and CLUT rectangles of the PLW onto an independent copy of the PLD VRAM. Does not rewrite the source files or polygon UV/TPAGE/CLUT values. Entirely static storage, C89 integer-only. The separate renderer accepts an optional companion PLD path.
2. **Guarded zombie damage remapping (new)**: `re3c_retarget_zombie_damage_15_to_11` checks the complete 11-joint rest hierarchy (including parent links and local positions) before applying the previously experimental 15→11 animation-channel map. Unrecognized rigs are rejected.
3. **Special-case diagnostics (new)**: `re3packed_special_audit` reports empty markers, out-of-range geometry streams, unusual face records, or plausible geometry. Does **not** silently repair uncertain assets.
4. **Regression tests (new)**: `test_vram_composite`, `test_zombie_damage`.

## Reproduce on Windows (MSYS2 MinGW32)

```bat
build_mingw.bat
re3packed_preview.exe PL00W00.PLW weapon_original.bmp quarter texture main native
re3packed_preview.exe PL00W00.PLW weapon_shared_vram.bmp quarter texture main native PL00.PLD
re3emd_anim_preview.exe EM16.EMD EM16.TIM zombie_damage 2 13 12 front texture model main rest zombie15to11
re3packed_special_audit.exe PL06CH.PLD
re3packed_special_audit.exe PL0D.PLD
```

Create the `zombie_damage` output directory before calling the animation tool.

## Measured results on the provided corpus

- **84/84 PLW**: tool successfully rendered the weapon geometry with shared PLD+PLW VRAM; no texture read failures, no entirely transparent outputs. These are rendering smoke tests, **not** proof of final weapon material fidelity or proper attachment to a player's hand.
- **13,552/13,552 PLW UV corners** returned a nonzero texel against the matching player TIM in the current corpus; **0** from the isolated PLW TIM at the unmodified TPAGE/CLUT settings. The standalone PLW pages therefore cannot be trusted as a complete texture source.
- **18 EMD** (11-joint zombie rigs with 15-joint damage banks) passed guarded mapping; **9,450** frame poses and a sampled skinned vertex were evaluated without errors.
- **17 EMD** have compatible native damage hierarchy; **24** others require different mappings; **41** lack a damage bank in this corpus.
- `PL0D.PLD`: exactly eight zero bytes; empty marker, not a normal model.
- `PL06CH.PLD`: 21 parts, with all 21 vertex-offset fields outside the MD2 section and 24 invalid conventional-triangle records. Its per-part vertex counts equal `PL06.PLD`; probable external geometry and a separate polygon encoding, **not safely decoded yet**.
- `PL0F.PLD`: 21-part geometry loads, but the solid-shaded view itself consists of broad rectangular panels. Not fixed by changing UV; functionality unknown.

## Limitations

- PLW attachment transforms / combined player-weapon rendering are **not yet implemented**.
- Matching nonzero pixels does not imply correct shader, blending or materials; most examples still look flat/very bright. Need reference captures or more tracing of Capcom's renderer.
- Damage remapping is **experimental and opt-in**; it is spatially justified for the reduced zombie rig, not verified with RE3 gameplay frame-by-frame. For non-zombie rigs, including Nemesis and Grave Digger, mismatched damage banks stay disabled.
- The PL06CH repair experiment was **not** accepted into the codec: borrowing PL06 vertices gave some valid data but did not recreate the special face encoding and produced zero visible triangles. No fabricated result is shipped.
- None of the original game executables, models, TIMs or textures are redistributed in this source ZIP.

## Quality

- ISO C89, `-pedantic -Wall -Wextra -Werror`, fixed-size arrays, integers/fixed-point, no heap.
- Build and unit tests executed on Linux GCC/Clang. Windows MinGW32 script updated but not executable on this Linux environment.
- Visual diagnostic previews are not the original Direct3D rendering or GTE behavior.
