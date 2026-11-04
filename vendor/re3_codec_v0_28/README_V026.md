# RE3 Codec v0.26 — guarded Nemesis / Grave Digger damage-bank investigation

This release is a reproducible research build based on v0.25; it does **not** claim frame-accurate Capcom damage animation playback.

## What was actually found

- EM34/EM35: 16-joint normal rig, 15-channel damage data, **7 clips and 446 keys**, same normal rig and identical damage EDD/EMR sections byte-for-byte.
- EM36: same 16-joint normal rig, but **8 independent damage clips, 324 keys**.
- EM30/EM33 (Grave Digger): 17-joint normal rig, 15-channel damage data, **1 clip, 40 keys**, identical damage EDD/EMR sections and normal rigs.
- Damage EMR `frame_offset=8` has no standalone hierarchy and `hierarchy_source=255`; the decoder can read the 15 rotations but cannot prove which normal-body joints they drive.
- Grave Digger damage channel activity: channels 1–7 are fixed throughout the 40-key clip; channel 0 changes twice; channels 8–14 contain most temporal motion. This does not identify what the channels mean anatomically.

## New C89 APIs

- `re3c_damage_family_check()` — checks full normal-rig parent+local rest coordinates, inverse DAT bind validation, and expected 15-channel damage format.
- `re3c_retarget_nemesis_damage_experimental()` — opt-in source channel projection to 16 joints; preserves previously explored prototype, now rejected on wrong rigs.
- `re3c_retarget_gravedigger_damage_experimental()` — opt-in 15-to-17 experimental projection, **neutral on two distal joints**. It is a visualization aid, not proof of a correct map.
- `re3c_damage_sections_equal()` — exact byte-for-byte EDD + EMR damage comparison in static 1024-byte blocks.
- `re3c_normal_rigs_equal()` — compare parent/rest positions of normal skeletons.

## New tools

```sh
re3emd_damage_compare EM30.EMD EM33.EMD
re3emd_damage_channels EM30.EMD
re3emd_damage_stress EM30.EMD grave
re3emd_damage_stress EM34.EMD nemesis
mkdir -p frames
re3emd_anim_preview EM30.EMD EM30.TIM frames 2 0 40 quarter texture model main rest grave15to17
# For Nemesis: same command with EM34, and "nemesis15to16"
```

All tools use fixed-size buffers, integer/fixed-point arithmetic, strict ISO C89 and no heap allocations. EMD and TIM files are not included in this source archive.

## Test scope

- Synthetic unit tests, plus full provided 100 EMD / 100 TIM / 100 EMD+TIM regressions.
- Full experimental damage projection and vertex-transform sweeps for EM34, EM35, EM36, EM30 and EM33.
- GCC and Clang C89 builds; `build_mingw.bat` is supplied for Windows but cross-compilation was **not** tested in this environment.
- No original game executables or resource files redistributed.

## Limitation (important)

The projection for Grave Digger compresses the body in some sampled poses. That is **not** a verified damage animation, but a sign that the source-channel assignment is not yet established. Nemesis sequences are visually more recognizable but remain unverified. We need original engine references/traces or a demonstrated 15-channel->body mapping before enabling either mapping by default.
