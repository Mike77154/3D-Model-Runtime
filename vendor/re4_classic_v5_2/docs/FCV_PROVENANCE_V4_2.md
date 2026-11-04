# RE4 Classic Codec v4.2 — Exact FCV provenance and special-channel inventory

## Source changes

* `animation/re4_motion_audit.c` and public `include/re4c3d/re4_motion_audit.h`: strict FCV role inspection; exact byte comparison of two **valid** FCV records; explicit cross-archive provenance gate. No heap, floats or external dependencies in C89 library.
* `examples/re4_motion_catalog.c`: report original FCV DAT indices and counts of rotation, root, IK, other special and unidentified channel blocks. Unknown/special channels are **not silently interpreted as ordinary rotations**.
* `model/re4_bestiaire.c`: `re4_beast_validate_set` checks entire part-set for the explicitly audited crow open-wing/closed-wing and candidate Bella-Isabel assemblies. Rejects duplicates, missing parts and wrong variants. A pass does **not** prove a scene-canonical assembly or native IK.
* `examples/re4_motion_provenance_audit.py`: optional offline research driver for user-owned `em.dat` and `xfile.dat`; produces complete per-index CSV and byte-identical comparison of Bella stage packs, without redistributing game assets.

## Actual corpus run (RE4 PC 2007)

We processed 16 selected families (`em10,em11,em12,em13,em15,em16,em19,em1a,em1b,em20,em23,em25,em29,em42,em45,em46`). All 2,314 FCV records in these DAT indexes were structurally readable. The selected packages contain 2,048 indexed SEQ entries. 2,283 FCV have a special IK channel and 2,174 have a root channel. **These channels are identified and preserved, not universally simulated.** 2,314 FCV entries are not the same thing as 2,314 unique game motions; duplicates and event/sound/phase control exist.

Bella stage family resources: 156 FCV indexed in each of `em16`, `em42`, `em45`, `em46`:

* `em42` vs `em46`: **156/156 identical**.
* `em42` vs `em16`: **141/156 identical**.
* `em45` vs `em16`: **156/156 identical**.
* The 15 modified indexes of `em42`/`em46` relative to `em16`/`em45`: 247, 252, 253, 255, 374, 651, 652, 654, 659, 660, 661, 662, 663, 664, 665.

These `emXX.dat` names can denote **level/location and gameplay packages**, not exclusive model or character definitions. The original context, head choice, weapon attachment and activation/event logic must be investigated independently. `em1203.pmd` is a documented Bella-Isabel head candidate, but the C89 set validation is still only candidate-level.

Head Plagas A/B/C remain UNRESOLVED. No verified three-way PMD-to-head-form and clip mapping was established from the provided archives; `em2500.pmd` is a locomotor Plaga, not automatic evidence for any of the three Ganado head forms. A safe runtime must refuse to guess. Environmental spider `em2e` is separate.

## Reproduction

```
make clean && make -j4 all && make check
./re4_motion_catalog /path/to/em42.dat > em42_fcv_roles.csv
python examples/re4_motion_provenance_audit.py \
  --xfile /path/to/xfile.dat --em /path/to/em.dat --out reports/
```

The source zip intentionally excludes original game files and generated assets. All 25 C89 test suites passed, including sanitizer builds. This is not equivalent to full all-frame reference rendering or Win32 validation.
