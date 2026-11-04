# RE4 Classic Codec v3.1 — Multi-character validation and retarget probes

**Goal:** make the RE4 PC 2007 codec test *all* available models, rather than special-case `pl00` (Leon) or `em10` (Don José).

This release retains the v3.0 progressive crossfade and previous archive/model/animation code. New code is **C89** in the codec and C89 diagnostic CLIs; the Python scripts are optional **external test orchestration**, not runtime dependencies.

## Verified against the user-provided archives

- `xfile.dat`: **1,789 PMD entries**, of which **1,789** passed native C89 read and rest-pose evaluation.
- **107** candidate character/resource families catalogued from `xfile.dat` / `em.dat` (some have no matching model or animation file).
- **62** families have a base PMD candidate and a correspondingly named DAT animation package.
- **54** families had at least one FK-compatible sample FCV in a bounded search of up to **5** candidate FCVs per family.
- **484** potential same-family part pairs examined: **278** with direct skeleton name links, **206** without. These are *candidates*, not guaranteed game-selected pieces.
- VTK proof with **three additional families**: `em11`, `em2c`, `pl05` using native C89 exports at frames 0 and 6. `em11` multipart also validated with main body, head, L/R hands. `pl05` multipart test includes its matching head; hand names did not pass direct-link heuristic.
- `em2c` (150 bones), `em38` (198 bones), `pl05` (186 bones), `pl06` (147 bones) and `pl09` (129 bones) previously failed the arbitrary 128-joint limit; all passed after changing the internal max to 512. A synthetic **200-joint regression test** is included.

This does **not** mean 1,789 playable characters or that every clip has been rendered: many PMDs are stage objects, weapons, effects and variant pieces. All resource entries were tested structurally in *rest pose*, with representative FK clips from matching packages. No claim of native IK, complete part-selection semantics, game-perfect morphs, character identification, original texture verification for these extra wireframe/shaded previews, or Win32 validation.

## New components

- `examples/re4_character_probe.c`: PMD rest-pose and optional FCV FK diagnosis, single CSV record, bounded caller-owned buffers.
- `examples/re4_part_probe.c`: optional multipart bone-link and retarget smoke test.
- `examples/re4_all_characters.py`: scans both original GCAXs without unpacking all game data permanently; CSV inventories and FK sample status.
- `examples/re4_all_parts.py`: checks candidate components across all character families.
- `examples/re4_vtk_family_samples.py`: creates VTK captures from C89-produced OBJ for independent model families.
- `examples/re4_auto_character_preview.py`: chooses only an exact base and some directly linked pieces for a *diagnostic* example, never claiming all candidate pieces are correct.
- `tests/re4_large_skeleton_test.c`: 200-joint FK regression.
- `RE4_SKEL_MAX_BONES=512`: used by the FK, retargeter, probe, clip exporters and candidate IK graph scan.

## Build

```sh
make clean && make -j4 all && make check
./re4_character_probe character.pmd
./re4_character_probe character.pmd animation.fcv
./re4_part_probe character.pmd possible_head.pmd
```

The optional Python harness needs Python 3, and the VTK example separately needs `vtk`:

```sh
python3 examples/re4_all_characters.py --xfile /path/xfile.dat --em /path/em.dat --out /path/results --clip-trials 5
python3 examples/re4_all_parts.py --xfile /path/xfile.dat --out /path/results/part_candidates.csv
python3 examples/re4_auto_character_preview.py --xfile /path/xfile.dat --em /path/em.dat --family-csv /path/results/character_families.csv --parts-csv /path/results/part_candidates.csv --families em11,pl05 --out /path/results/VTK
```

The scan writes `all_pmd_validation.csv`, `character_families.csv`, `animation_packages.csv`, `fk_samples.csv`. The source package **does not include any copyrighted PMD/FCV/TGA game bytes**. Inputs remain in their original GCAX archives; only bounded temporary extracts are created by the Python harness and deleted afterward.

## Validation boundary

- 13 synthetic test executables pass on Linux using GCC `-std=c89 -Wall -Wextra -pedantic`.
- The full PMD corpus passes **rest-pose FK**; this is a mechanical decoder and matrix-hierarchy test, not a visual completeness guarantee.
- Additional VTK captures use neutral material to expose topology, not original texture maps. `em2c` head/auxiliary model does not share directly matching named bones with the selected master; it remains unassembled in sample preview.
- FCV compatibility is currently inferred from at least one usable clip in the initial samples, not every FCV/SEQ and not semantic mapping to named actions.
- Big-endian GameCube/Wii and PS2 rendering, morphs and original IK are outside these new tests.
- Fresh compilations are required when changing public headers/struct sizes. Always run `make clean` before building this version.
