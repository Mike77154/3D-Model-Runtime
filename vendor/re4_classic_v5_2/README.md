# v5.2 — Deterministic SEQ-to-visual-state bridge (2026-11-02)

- `re4_seq_state` C89: **caller-declared**, non-native action mappings for
  visibility, pose IDs and parent IDs (32 parts). Atomic transactional event
  replay, strict unknown-event policy, bounded loops and reproducible seeking.
- `re4_seq_state_inspect`: CLI accepting *explicit demonstration* rules; neither
  native meanings nor canonical SEQ/FCV ownership is inferred.
- QA on user-owned `em.dat`: 3,403 uniquely time-matched FCV/SEQ candidates
  across 148 families, 3,403 C89 two-loop replays with independently verified
  raw event counts. 33 strict C89 suites passed with and without ASan/UBSan.
- `evd.dat`: 156/159 EVD packet sections inspectable, no SetParts/SetEm/Mot
  among those; EVD corpus is not a proven head-Plaga model mapping.
- See [`docs/SEQ_VISUAL_STATE_BRIDGE_V5_2.md`](docs/SEQ_VISUAL_STATE_BRIDGE_V5_2.md).

---

# v5.1 — Evidence-based FCV/SEQ association (2026-11-02)

- C89 native `re4_seq_pair_probe` and `re4_seq_pair_dispatch`: cross-check full FCV and SEQ input, 10.6 time ranges, frame-derived period, explicit resource fingerprints and fail-closed rejection of incompatible pairs.
- `re4_v51_pair_candidates.py`: 12,289 SEQ sorted into unique, ambiguous and non-exact timing categories across 176 original DAT families; one candidate per 148 families independently verified via C89. **Not a claim of canonical FCV/SEQ pairing or resolved gameplay event meanings.**
- New synthetic unit suite; source-only package retains v5.0 functionality. See [`docs/SEQ_FCV_PAIRING_V5_1.md`](docs/SEQ_FCV_PAIRING_V5_1.md).

---

# v5.0 — SEQ raw event scheduling and full `em.dat` replay audit (2026-11-02)

- New native `animation/re4_seq_dispatch.c`: deterministic half-open event
  windows, ordered ties, loop-safe replay, bounded loops and callback errors.
  **No guessed game event semantics or SEQ-to-FCV pairing.**
- New `re4_seq_replay` and `re4_seq_replay_audit` C89 tools, plus regression test.
- Optional external Python corpus QA scripts: `re4_v50_replay_corpus.py`
  and `re4_v50_seq_codebook.py`. The runtime itself remains pure C89.
- Real corpus: 176 nested `em` DAT scanned; 12,289/12,289 SEQ passed event
  replay invariants; 183,621 nonzero-coded keys, 50 distinct code bytes.
- See `docs/SEQ_DISPATCH_V5_0.md` for limitations, commands and status.

## v4.9 — SEQ corrected binary layout + whole-corpus event timeline audit

Critical fix: RE4 SEQ keys use `{event:u8, aux:u8, frame_q6:u16}` (not `{frame_q6:u16, event:u8, aux:u8}`). New `re4_seq_timeline_inspect`, half-open `re4_seq_timeline_window`, and `re4_seq_dat_audit` C89 tool. Tested 12,289 original SEQ / 841,708 keys across 176 nested DAT packages without malformed sequences. Full results and limitations in `docs/SEQ_TIMELINE_V4_9.md`. This does **not** reconstruct native event semantics or resolve Plaga head variants. 30 C89 suites passed including sanitizers. Source distribution excludes all game assets.

# RE4 Classic Codec v4.8 — rig master `born1__` y 19/19 conjuntos candidatos

- Arreglado el enlace FCV/PMD de Armadura A (`em3c00` + `em3c01`); 55/55 FCV muestreados OK.
- Méndez fase 2 (`em3501a` + `em3501b`): 83/83 FCV muestreados OK bajo revisión de jerarquía; su montaje visual todavía requiere eventos nativos.
- 19/19 conjuntos multipartes candidatos pasaron pruebas de malla muestreadas; no implica cobertura canónica de todas las criaturas.
- Salazar mutado FCV 42: evaluados los 251 fotogramas, sin saltos consecutivos mayores de 250 unidades de modelo.
- Nuevo test C89 `re4_born1_master_test`; **29 suites** superadas, también ASan/UBSan.
- Detalles, limitaciones y comandos en [`docs/ENEMY_COVERAGE_V4_8.md`](docs/ENEMY_COVERAGE_V4_8.md).

---

# RE4 Classic Codec v4.7 — Auditoría masiva de mallas FCV

Nuevo `--summary` nativo C89, banco de pruebas por familias, inventario integral de FCV y selección de PMD alternativo comprobada. Véase `docs/RE4_CORPUS_MESH_V4_7.md`. Sin assets del juego en el ZIP.

# RE4 Classic Codec v4.5 — Shared FCV bone mapping and whole-frame FK audit

The compatibility inspector now shares the **exact same** FCV joint identifier resolution as the skeletal evaluator, including large joints 100–255 and bird/bat model-specific prefixes. Previously it could produce false negatives for crow/bat models and false mappings for skeleton bone `born__100`. `re4_clip_sequence_audit` evaluates every integer frame of a user-provided FCV against a PMD (no heap/no OBJ output), fails closed when rotation channels cannot bind, and reports numeric continuity indicators. The 91-clip / 7,679-frame user corpus audit is in the accompanying *separate* report ZIP, not in this code archive. This is FK validation, NOT full IK/root/morph/texture or certified game animation. See `docs/WHOLE_FRAME_COVERAGE_V4_5.md`.

---

# RE4 Classic Codec v4.4 — external mods and manual rigid accessory attachments

**New, tested C89 capabilities:** standalone `R4MOD1` mod-package manifest parser, `re4_mod_probe` offline structural checker for loose files from PC 2007 (PMD + FCV + TGA references) and preliminary loose UHD BIN support, an explicit `re4_character_rigid_anchor()` API, and an opt-in `--rigid-anchor=PART:JOINT` exporter flag. These are *diagnostic tools*, not full RE4-native IK or proof of correct parts, and not an import path for arbitrary non-RE4 3D files. See `docs/MOD_SUPPORT_V4_4.md`.

The v4.3 census (107 families, 11,082 readable FCV) is retained. Unresolved enemy and Plaga-head mappings are **not** marked complete. No original game/mod assets shipped.

---

# RE4 Classic Codec v4.3 — Enemy coverage and multi-part presence guards

- Adds explicitly checkable PMD *named-set* candidate profiles for U-3, Saddler spider, Mendez two phases, Novistador three geometries, Regenerador, Iron Maiden, both Armaduras and two Gigantes. All file-name sets **require additional skeleton, clip, texture, VTK checks** before considered usable; these are not canonical claims.
- Includes comprehensive user-corpus scanning driver `examples/re4_enemy_universal_audit.py`: all em/pl packages in supplied GCAX em.dat, model archive xfile.dat, independent FCV role analysis, SEQ counts, PMD-family presence, archive reuse/identical clip grouping, and missing-model flags. Original game resources never included in distributable ZIP.
- Three head-Plaga A/B/C PMDs are still unresolved. The script does NOT rename any em25 locomotor parasite into head type A/B/C.
- Existing native C89 code remains heapless, fixed-point Q16.16, and public API compatible.

# RE4 Classic Codec v4.2 — Verified FCV provenance and multipart completeness

**New:** all 25 C89 test suites pass; added role-aware FCV audit, exact-byte provenance policy, and whole-assembly checks for crow and candidate Bella variants. The 16 selected real game families yielded 2,314 structurally readable FCV, and 2,048 indexed SEQ. The Plaga head A/B/C model and motion mappings are **not yet confirmed**, and native IK is not yet faithfully emulated. See [`docs/FCV_PROVENANCE_V4_2.md`](docs/FCV_PROVENANCE_V4_2.md).

---

# RE4 Classic Codec v4.1 — Bird/Bat FCV aliases & bestiary resource audit

**New in v4.1:** Crow and bat FCV rotation channels now bind to their observed PMD skeleton identities; dedicated crow wing retarget aliases, strict variant guards, three separate unresolved Plaga head profiles, and optional cross-family Bella FCV comparison. See [`docs/BESTIARY_V4_1.md`](docs/BESTIARY_V4_1.md). **24 C89 tests pass** including the two new test suites. Native IK, all-frame visual equivalence, complete Plaga A/B/C head identification and Win32 integration remain open.

---

# RE4 Classic Codec v4.0 — Bestiario, perfiles de criaturas y variantes

Actualización acumulativa de la v3.9. Añade 39 perfiles al núcleo C89,
validador de variantes multipartes, auditoría de 330 modelos candidatos y
pruebas específicas de Regenerador/Iron Maiden. Consultar
`docs/BESTIARY_V4_0.md`. El ZIP fuente NO incluye PMD, FCV ni TGA de Capcom.
**La cobertura del catálogo no demuestra que todos los modelos ni sus clips
se reproduzcan fielmente.**

---

# RE4 Classic Codec v3.9 — PMD weights and multi-character diagnostics

Cumulative v3.8 source. New C89 `re4_pmd_weights` diagnostic, full-corpus `examples/re4_weight_corpus.py`, tested assembly profiles in `examples/re4_creature_presets.py`. See [docs/PMD_WEIGHTS_V3_9.md](docs/PMD_WEIGHTS_V3_9.md). These are **not** certifications of game-native character, animation, IK, or materials. Source ZIP contains no original models, textures or clips.

---

# RE4 Classic Codec v3.8 — multipart character repair (C89)

**New in v3.8:** Luis's hands no longer float in sampled FCV poses: the hand-specific `pl04lh04__NN`/`pl04rh01__NN` joints are now mapped onto his main skeleton with a narrow duplicate-target exemption. Wesker's `pl0d` preview now includes `pl0d0e.pmd` for his legs. The Ada/Krauser/Salazar VTK regression samples still pass. See [docs/CHARACTER_ASSEMBLY_V3_8.md](docs/CHARACTER_ASSEMBLY_V3_8.md) for exact sample scope, 21 passing C89 suites, safety checks, and remaining limitations. No game assets in this package.

---

# RE4 Classic Codec v3.6 — character attachment diagnostic, verified C89 changes

**Source distribution only:** does NOT include Capcom's original PMD/FCV/TGA resources.
This release is cumulative from v3.5.

## Actual source changes
- C89 bone-name matcher recognizes the observed `LHAND__NN_*` / `RHAND__NN_*` identifiers of Ada `pl0b` hand models, connecting them to the master `born__NN` joint identities. For example `LHAND__15_S09` maps to `born__15` and `RHAND__09_S15` maps to `born__09`. Other arbitrary names are not accepted.
- New `re4_character_check_policy()` API, default strict; alternative `RE4_PART_POLICY_REVIEW_ANCESTRY` may be selected only deliberately. Duplicate direct targets, missing direct links, and invalid targets still fail.
- Exporter supports `--review-ancestry` **only together with** `--strict-parts`, emits an explicit warning for each reviewed mismatch, and retains `--require-part` fail-closed checks. This mode is experimental world-space retargeting, not proof of canonical skeleton matching.
- `re4_part_audit MASTER PART --dump-bones` emits every part joint, its parent and master target to stderr while preserving the CSV summary on stdout.
- New C89 tests: `re4_hand_alias_test` and `re4_part_policy_test`. A repeatable optional Python/VTK source-only capture utility is in `examples/`.

## Verified real-file results (RE4 PC 2007 user supplied)
- Ashley `pl01`: 4 PMDs / 13 TGA resources; 47 VTK actors; FCV index 24 frames 0, 3, 6. Strict structural linking.
- Ashley `pl05`: 4 PMDs / 11 TGA resources; 66 VTK actors; FCV index 24 frames 0, 3, 6. Hair has 12 ancestry differences and requires `--review-ancestry`; visually inspected on those three frames, NOT certified for every motion.
- Ashley hair files `pl0101.pmd` and `pl0501.pmd` have identical SHA-256 bytes, but their master-body hierarchies differ. This is an important demonstration that structural conflicts can reflect different parent schemes, rather than broken file data.
- Ada `pl0b`: 5 PMDs (pl0b0a,0b,0c,lh00,rh00) / 9 TGA; 31 VTK actors; FCV index 24 frames 0 and 6. Left and right hand links now each resolve 2 direct joints under strict mode. Visible seam/edge around hair remains; assembly choice is still a **candidate**, not confirmed canon.
- Saddler `em3f`: 4 PMDs (00,01,lh00,rh00) / 7 TGA; 67 VTK actors; FCV index 10 frames 0 and 6. Structural candidates, no detected hierarchy conflict in selected parts; not every boss variant yet.
- Salazar `em38`: body+head load and permit reviewed ancestry but **exporting animated geometry FAILS** at `part=0 node=66 vert=0` with `RE4_ERR_BAD_FORMAT` in the current skinning path. We intentionally did not produce a misleading screenshot.
- Luis and Krauser: master-model identity / multipart mapping not verified. Unresolved, not claimed repaired.

## Build and test
```
make clean && make -j4 all && make check
```
All **20 C89 test suites** pass with GCC `-std=c89 -Wall -Wextra -pedantic`, and also under ASan/UBSan in a separate clean build. Linux only tested; no Win32 or real-time runtime integration verified.

## Reproduce with extracted user-owned assets
```
./re4_character_clip pl0500.pmd motion.fcv 0 6 3 out \
    pl0501.pmd pl05lh00.pmd pl05rh00.pmd \
    --strict-parts --review-ancestry \
    --require-part=pl0501.pmd \
    --require-part=pl05lh00.pmd --require-part=pl05rh00.pmd
```

For Ada, the discovered bone aliases are handled in strict mode; no hierarchy-review override is required for the tested hand components.

## What remains
- Complete unambiguous visual identification/assembly for Ada variants and all named bosses, Ashley pl17, Luis, Krauser.
- Fix Salazar node-66 PMD skinning error with raw vertex/bone inspection.
- Blend/IK/alpha/LOD full-sequence validation for every character; runtime Win32 integration.
- Captures are renderer outputs from extracted PMD/TGA and FCV, **not image generation**. Source ZIP contains no game assets.

---

## v3.5 — Character completeness guard and Ashley hand bone prefixes

This update targets the "ghost clothes / missing hair or hands" failure mode.
It **does not** claim every RE4 PC 2007 character is completely assembled.

- The C89 skeleton matcher now recognizes part-local bone identifiers
  `born1__NN` and `born2__NN` (used by Ashley's hand PMDs), in addition
  to `born__NN`, `lh00__NN`, `rh00__NN`. It deliberately does *not*
  automatically remap `born3__` helper bones without evidence.
- New `re4_required_parts_check()` C89 public API and `--require-part=NAME.pmd`
  CLI flag, repeated as needed. Missing, duplicated or ambiguous required
  filenames stop an export **before any incomplete pose is written**.
  Use this alongside `--strict-parts` for skeleton safety.
- `examples/re4_character_completeness_audit.py` reports candidate pieces
  and structural compatibility for Ashley, Ada, Salazar and Saddler, while
  explicitly marking the principal Luis/Krauser models unidentified.
  A matching texture name is NOT proof of canonical character identity.

### Real Ashley `pl01` test

Actual user-provided `xfile.dat` / `em.dat`:
- `pl0100.pmd` master; `pl0101.pmd` hair; `pl01lh00.pmd` left hand;
  `pl01rh00.pmd` right hand.
- Bone audit: hair 51 direct, left hand 2 direct, right hand 3 direct;
  no duplicate-direct or ancestry errors for these three parts.
- Two FCV frames (0 and 2, from `em\pl01.dat` FCV index 24) exported
  with 4 PMDs and 14,832 vertices per frame.
- Negative test omitted right hand; exit code 2; NO pose output.
- `pl05` hair has 12 hierarchy conflicts and remains REVIEW_REQUIRED;
  do not force it or silently switch to `pl01` parts.

Example (use asset files extracted by the user from their own archive):

```sh
./re4_character_clip pl0100.pmd clip.fcv 0 2 2 ashley_poses \
  pl0101.pmd pl01lh00.pmd pl01rh00.pmd \
  --strict-parts \
  --require-part=pl0101.pmd \
  --require-part=pl01lh00.pmd \
  --require-part=pl01rh00.pmd
```

### Boundaries

C89 core uses caller-supplied storage, no malloc and no floats.
Python is only an optional offline auditing utility.
The exact attachment of every Ada, Salazar, Saddler, Luis and Krauser variant,
IK and visual motion continuity are **not yet verified**. In particular,
`pl02` head/hair candidates and `pl05` hair need stronger mapping evidence.
Linux C89 build and 18 automated suites pass. The new guard and a real
Ashley clip export also pass ASan/UBSan. Win32 is not yet verified.

---

# RE4 Classic Codec v3.5 — safe multi-character bone-link audit

A cumulative, self-contained C89 source update built from v3.3. The core is
strict ANSI C89, Q16.16, fixed caller-owned memory, and no third-party runtime
libraries. Python/Pillow/VTK are optional **offline diagnostic tools**, not
linked into the C89 codec. Original game assets are **NOT bundled**.

## v3.4: validated character components (2026-11-01)

This release is cumulative over v3.3. The **native C89** character
assembler now parses complete multi-digit bone names, so identifiers like
`born__10` and `born__100` cannot collide. It continues to accept suffixes
such as `born__10_S16` after the complete decimal identifier.

New `re4_character_audit()` checks part-to-master links for:

- Duplicate direct targets, which can double-map independently named joints.
- Parent-ancestry conflicts when a named part bone is not a descendant of
  its directly mapped ancestor in the master skeleton.
- Missing or out-of-range targets and stationary/unmapped bones.

`re4_part_audit` performs this check on actual PMD pairs without any Python
parsing of bones. `--strict-parts` on `re4_character_clip` **opts in** to
reject parts failing these constraints before creating output files. The
previous permissive workflow remains available for manual research.

The command-line clip exporter now accepts PMD files up to **16 MiB** each
(previously 4 MiB), needed for some complex models. The C89 library remains
heap-free and the CLI uses fixed-capacity buffers, with explicit errors for
oversized input.

### Measured against the original user-supplied xfile.dat

484 previously indexed potential bodypart pairs were re-evaluated using the
new native `re4_part_audit` reader:

- **269** structural candidates with direct matches and no detected duplicate
  target or ancestor conflict.
- **206** without any directly corresponding bones.
- **9** require hierarchy review (includes several `em38` variants, `em35`,
  `em3a`, `em3f`, `pl00`, `pl05`).
- 83 exact head/left/right candidate rows are reported separately; 49 rows
  are single structural candidates and 34 have no safe match in this pass.

The term 'structural candidate' is **not** proof of the original game's
canonical attachment choice. Some original part skeletons may have legitimately
reparented bones or special IK constraints; the 9 flagged pairs are for
manual study, not automatically declared corrupted. The scan is of the
previously catalogued 484 candidates, not an exhaustive new search of all
possible PMD pair combinations.

### Repeatable tests

```sh
make clean && make -j4 all && make check
./re4_part_audit master.pmd candidate.pmd
./re4_character_clip master.pmd - 0 0 1 output_dir part.pmd --strict-parts
python3 examples/re4_part_audit_all.py \
  --xfile /path/to/xfile.dat \
  --parts-csv /path/to/part_candidates.csv \
  --out /path/to/reports
```

17 test suites pass with GCC C89 `-Wall -Wextra -pedantic` (zero build
warnings), and the 17 also pass under ASan and UBSan. The real `em11`
master/head example succeeds with `--strict-parts`; the `em38` head candidate
is correctly blocked for 4 ancestor conflicts. Tests executed on Linux;
Win32 and game-perfect IK/LOD/UV/transparency are not yet verified.

**No original game-model or texture bytes are embedded in this ZIP.**

---

## What changed in v3.3

- New `re4_pmd_texture_key()` API in `model/re4_pmd_material_name.c` / `re4_model.h`.
  Bounded, ASCII case-insensitive basename normalization of PMD texture refs;
  does not open or write paths. It rejects malformed names, control bytes and
  output truncation. Original PMD material references are retained.
- New C89 `re4_pmd_materials` CLI emits one CSV row per PMD material with its
  original texture enabled flag, safe lookup key and reference state.
- New `examples/re4_material_assembly_audit.py` catalogs ALL supplied xfile.dat
  PMDs and maps actual material references to indexed TGA archive entries:
  same-folder / unique elsewhere / ambiguous / missing. Emits a conservative
  multipart candidate manifest using existing `re4_part_probe` evidence.
- New `examples/re4_tga_validate.py` independently *decodes* TGA images using
  Pillow rather than just trusting the index or extension.
- New `examples/re4_manifest_vtk_preview.py` demonstrates textured
  multi-piece animation poses for families beyond pl00/em10, through the
  existing **C89** `re4_character_clip` and the optional **VTK** importer.
  It preserves case-sensitive texture filenames so Linux can load them.
- New `re4_material_name_test` brings the `make check` suite count to 15.

## Real archive validation (xfile.dat / em.dat supplied by user)

- 1,789 / 1,789 PMD files parsed with the new material inspector.
- 9,438 PMD material records. 9,437 explicitly name a TGA, and 1 has no
  texture name. Every named TGA was found **in that model's own archive folder**.
- 1,564 / 1,564 TGA images load completely in Pillow (1,269 RGB / 295 RGBA).
- 51 conservative bodypart candidate bindings for 31 families: 29 head,
  11 left-hand, 11 right-hand. These are *filename/skeleton candidates*, not
  automatic confirmation of RE4 canonical variants.
- New real VTK textured previews: `em12` (4 PMD, 3 textures, 17 actors)
  and `pl05` (2 PMD, 10 textures, 64 actors), at sampled FCV frame 6.
- 15 C89 suites pass, standard build and an independently rebuilt ASan/UBSan
  instrumented build. Linux verified; Win32 not yet verified.

Do not interpret these results as proof of canonical character assemblies,
correct UV/alpha/morph/IK semantics, or game-equivalent motion. This is the
PC 2007 corpus; GameCube/PS2 decoding needs its own validation.

## Build

```sh
make clean && make -j4 all && make check
./re4_pmd_materials your_character.pmd
python3 examples/re4_material_assembly_audit.py \
  --xfile /path/to/xfile.dat \
  --parts-csv /path/to/part_candidates.csv \
  --out /path/to/reports
python3 examples/re4_tga_validate.py --xfile /path/to/xfile.dat \
  --out /path/to/reports/tga_validation.csv
```

Optional VTK diagnostic:

```sh
python3 examples/re4_manifest_vtk_preview.py \
  --xfile /path/to/xfile.dat --em /path/to/em.dat \
  --families /path/to/character_families.csv \
  --manifest /path/to/reports/multipart_manifest_candidates.csv \
  --materials /path/to/reports/pmd_materials.csv \
  --out /path/to/previews --families-list em12,pl05 --frame 6
```

The Python scripts use original user-provided assets **only in temporary
folders**, emit only CSV / PNG and never include game files in the source ZIP.

## Next targets

- Exact material transparency/alpha and UV validation in **all** families.
- Authentic part-graph and LOD/variant selection by gameplay state, rather
  than filename candidate heuristics; warn rather than silently guess.
- Per-character full FCV/SEQ playback, native IK, morphs and true Win32
  runtime adapter.

## v4.6 — Deformed mesh sequence auditor (C89)

Adds `re4_mesh_sequence_audit` to measure **actual skinned PMD vertices**, not only FK bone matrices, at every selected FCV frame. Works with a declared master and up to seven PMD parts, using audited world-space retargeting. It validates PMD vertex decoding and face indices, skinning status, bounding extents, changed vertex counts, maximum component-wise per-vertex frame-step (Q16.16), and fixed-point near-limit positions. Output is CSV to stdout; diagnostics and an explicit summary go to stderr.

```sh
make re4_mesh_sequence_audit
./re4_mesh_sequence_audit em2300.pmd em23_clip.fcv 0 40 1 > crow_mesh.csv
./re4_mesh_sequence_audit pl0100.pmd pl01_clip.fcv 0 40 1 \
    pl0101.pmd pl01lh00.pmd pl01rh00.pmd > ashley_mesh.csv
```

Options: append `--review-ancestry` to permit *explicitly reviewed* noncanonical parent topology. No hidden/automatic rigid attachment is used. A mismatched part fails closed. A return code 7 means an alert (displacement threshold >250 model-units per selected step or a nearly saturated coordinate) and does not by itself prove malformed original data. Exit 0 means no audit exception or threshold alert, **not** native motion fidelity. Other nonzero codes indicate invalid input or a loader/retarget/skin failure. Fixed buffers cap at 8 PMD components, 400,000 vertices per frame, 16 MiB per PMD, 2 MiB FCV, and 8192 sampled frames; caller can use multiple runs for longer clips.

Restrictions: PC 2007 PMD/FCV little-endian only; FK only (no exact game-native IK, morphing or root-controller application); source model and clip must be selected correctly. Identical textures, correct body-part identity and in-game state switching are **not certified** by this audit. This tool does not read the game archives directly: use `re4_gca_catalog` and `re4_fcv_extract` first. User-supplied game data is not included in the distributed code ZIP.

The v4.6 validation report documents real models `em12`, `em23`, `em25`, `em29`, `em38`, `em3c`, multipart Ashley `pl01`, and an intentionally incompatible `em3c`+`em12` part that is correctly rejected.
