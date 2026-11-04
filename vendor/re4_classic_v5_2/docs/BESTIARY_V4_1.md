# RE4 PC 2007 v4.1 — bird/bat/plaga and Bella FCV resource audit

## Verified fixes in C89 core

- `re4_skel_joint_for_fcv_id()` accepts the **observed** `em2300_crow__NN` and `em2900_bat__NN` bone-name stems in addition to `born__NN`, with duplicate-ID rejection. This changes **FK linking**, not the binary FCV decoder.
- Crow multipart retargeting recognizes `em2301_openWing__NN` and `em2302_closeWing__NN` **only** against `em2300_crow__NN` skeleton identities; it does not generically attach arbitrary similarly numbered bones.
- Bestiary guards distinguish `cuervo-alas-abiertas` (body + open wing), `cuervo-alas-cerradas` (body + folded wing), `plaga-arana` (em25), and environmental spider `arana-ambiente` (em2e).
- The three *Ganado head Plaga A/B/C* forms remain **unmapped** to distinct verified PMD names. `em25` is explicitly documented as locomotor Plaga Araña, NOT as proof of the three head parasites. The runtime must not automatically substitute it for head variants.
- New `bella-isabel-candidate` profile explicitly permits `em1200`, documented Bella Sister head `em1203` and the neutral `em12` hands, but is labeled **candidate** because there is no per-room original canonical assembly validation.

## From the user-provided xfile.dat and em.dat

- Crow `em23`: 3 PMD, 19 FCV, 17 SEQ. **19/19 FCV** link to main body FK; `em2301`/`em2302` alone have 0 direct bone rotations in the reader. Tested multipart assemblies of *body + one wing variant* on frames 0 and 6, textured VTK, not a game-original flight reference.
- Bat `em29`: 1 PMD, 18 FCV, 1 SEQ. **18/18 FK-compatible** after prefix fix. Textured VTK frames 0 and 6.
- Locomotor Plaga `em25`: 1 PMD, 32 FCV, 12 SEQ. **32/32 FK-compatible**. Textured VTK frames 0 and 6.
- Environmental spider `em2e`: 2 PMD; **no FCV/SEQ package** identified in this `em.dat`. Its animation might be script-driven or stored in other archives, but that is not demonstrated here.
- Three Bella Sister motion archives `em42`, `em45`, `em46` each have **156 FCV and 145 SEQ**; this corpus has no matching family PMD in xfile.dat.
- Comparing FCV bytes by matching DAT index with `em16`: `em42` has **141/156** exact matches, `em45` **156/156**, `em46` **141/156**. The 15 differing indices in `em42` and `em46` are preserved in `bella_different_fcv_indices.csv` for further research.
- All **468/468** Bella-family FCV from these three archives generated linked FK rotations on the **candidate** `em1200.pmd` skeleton using `re4_character_probe`. This does **not** prove that their animations are equivalent to the canonical Bella Sisters, nor that IK, weapon and gameplay events are reconstructed.
- A *candidate* four-PMD `em1200+em1203+em12lh00+em12rh00` exported a real `em42` FCV at frames 0 and 6 to VTK, with textures. No chainsaw or canonical scene layout was validated.

## Validation

`make -j4 all && make check` runs 24 suites; two new regression suites cover bone IDs and safe crow wing linking. Targeted `-fsanitize=address,undefined` builds of new tests passed. Actual original-resource read and VTK exports were performed. This is not a global full-frame golden master and not Win32-tested.

## Reproduce

```
make -j4 all && make check
python examples/re4_focused_resource_audit.py --xfile /your/xfile.dat --em /your/em.dat --out reports/
python examples/re4_bella_crossfamily_fk.py --xfile /your/xfile.dat --em /your/em.dat --out reports/bella_crossfamily.csv
python examples/re4_character_family_preview.py --xfile /your/xfile.dat --em /your/em.dat --family em23 --models em2300.pmd,em2301.pmd --fcv 5 --out renders/crow_open --profile cuervo-alas-abiertas --review-ancestry
```

Original RE4 game files are **not** embedded in the source distribution.
