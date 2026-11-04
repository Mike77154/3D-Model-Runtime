# RE4 Classic Codec v2.7 — multipart full-clip playback + IK diagnostics

## Verified features

- `re4_fcv_extract` reads RE4 2007 nested LE DAT and writes every validated FCV to numbered files; source bytes remain unmodified. It also makes `catalog.csv` with the index, duration and channel count.
- `re4_character_clip` reads one master PMD, one FCV and up to seven accessory PMDs **once**, then evaluates a requested frame range. Every frame exports a complete multipart OBJ + MTL from the C89 fixed-point skinner. `poses_index.csv` logs frames, part counts and IK metadata. No malloc, floating-point, external library or VTK dependency in this executable.
- FCV `0x10`/`0x20` joint kind flags are identified as *IK root candidates*, including bend-axis code (`data_type & 0xF`), potential extra toe/four-joint flags, and child-chain topology. A topology match is **not a confirmed game chain**.
- Optional `--ik-lock` uses the new axis-limited world-space CCD solver; effector targets come from the selected start frame pose, not from the game's native IK target decoding. This is a **diagnostic contact-lock approximation only** and is intentionally off by default.
- `re4_character_link` now finds the nearest mapped ancestor even when there are intermediate unnamed bones.
- `examples/re4_vtk_multipart.py` renders OBJ/MTL/TGA frames via VTK, with no image generation. Python/VTK/Pillow are optional and never linked to the C89 codec.
- `examples/re4_all_clip_driver.py` optionally drives the C89 exporter over a folder of FCVs (Python standard library, bounded by default). Writing every frame for every clip can require many gigabytes; run small batches first.

## Proven real-data tests

All below are from user-supplied RE4 PC 2007 data; source package intentionally **does not include** original game models, animation files, or textures.

- `pl00.dat`: 65 FCV validated, 1 empty marker; extracted by `re4_fcv_extract`.
- `em10.dat`: 277 FCV validated; extracted by `re4_fcv_extract`.
- `pl0000.pmd` + `pl0001.pmd` + `lh00.pmd` + `rh00.pmd`: 14,751 vertices in each exported frame; one FCV from frame 0 to 40 at step 1 (41 frames).
- `em1000.pmd` + `em1001.pmd` + `em10lh00.pmd` + `em10rh00.pmd`: 3,729 vertices in each exported frame; one FCV from frame 0 to 184 at step 4 (47 frames).
- VTK 9.6.2 loaded real exported OBJ/MTL plus extracted TGA: 55 actors for Leon, 14 for Ganado. Preview PNG/GIF validated separately.
- Synthetic constrained IK test: L1 effector error reduced from 131072 to 35754 in Q16.16 units; not a game-accuracy benchmark.

## Build and example usage

```sh
make -j4 all
make check
./re4_fcv_extract pl00.dat fcvs_leon
./re4_fcv_extract em10.dat fcvs_ganado
./re4_character_clip pl0000.pmd fcvs_leon/clip_0024.fcv 0 40 1 poses_leon pl0001.pmd lh00.pmd rh00.pmd
./re4_character_clip em1000.pmd fcvs_ganado/clip_0001.fcv 0 184 4 poses_ganado em1001.pmd em10lh00.pmd em10rh00.pmd
# Optional: experimental contact lock (NOT native RE4 IK)
./re4_character_clip pl0000.pmd fcvs_leon/clip_0024.fcv 0 40 4 poses_ik pl0001.pmd lh00.pmd rh00.pmd --ik-lock
# Optional automation; defaults to 5 clips and a 4-frame stride:
python3 examples/re4_all_clip_driver.py --tool ./re4_character_clip --extracted fcvs_leon --master pl0000.pmd --part pl0001.pmd --part lh00.pmd --part rh00.pmd --out all_leon
# Actual full archive export, explicitly opted in:
python3 examples/re4_all_clip_driver.py --tool ./re4_character_clip --extracted fcvs_leon --master pl0000.pmd --part pl0001.pmd --part lh00.pmd --part rh00.pmd --out all_leon --all-clips --stride 1
# Optional VTK renderer after supplying the original referenced TGAs into the pose directory:
python3 examples/re4_vtk_multipart.py poses_leon 0 6 12 18 24 30 36 40
```

## Limits still open

- These are *full local-frame ranges for each FCV*, not proof of all game-context animations. Room/event/weapon clips, blending, player-state transitions, SMD/SEQ effects and facial morphs are not combined into one authoritative gameplay timeline.
- `re4_skel_evaluate` applies rotation tracks of verified type 2; root displacement, scale, IK targets and special channels are logged but not automatically executed. The world-space CCD option does not implement native IK `ikCalc`, heel/toe constraints, bend-plane orientation or native chain blending.
- Two-influence skinning, multi-part retargeting and material associations are functional for tested examples, not universal validation for every RE4 character variant.
- The native-game comparator and Win32 runtime integration have not been validated yet. No native runtime/GPU player is included; the OBJ/MTL export is offline and VTK is diagnostic.
