# RE4 Classic Codec v4.0: bestiary registry and fail-closed variant guard

This is an extension of v3.9 **for RE4 PC 2007 PMD/FCV assets**. The core is C89, Q16.16, caller-managed storage. Source distribution does not include original game data. The C89 family registry is data-grounded, but a family ID does not prove that a particular skin/face/hand/weapon variant was used by the game's original character configuration.

## Added in v4.0
- `include/re4c3d/re4_bestiaire.h`, `model/re4_bestiaire.c`: 39 categorized diagnostic profiles. Known unknowns (Bella Sisters, plaga A/B, bull) have no invented family.
- Special `em36` guard distinguishes `em360*` Regenerador from `em361*` Iron Maiden. Likewise recognizes **verified pair candidates** for `em2b` Gigante and `em3c` suits of armor. Unrecognized same-family variants return REVIEW, not ACCEPT.
- `re4_bestiaire_catalog --all | --profile SLUG | --part SLUG FILE.pmd`; prints explicit REJECT/REVIEW/ACCEPT.
- `re4_character_clip --creature-profile=SLUG` guards the selected master and component filenames **before resource loading/output**. This does not replace the preexisting `--strict-parts`, `--require-part`, bone audits, or full clip tests.
- `examples/re4_bestiary_audit.py` runs the native registry over previously generated corpus PMD-weight and family/FCV CSV reports and writes three new summary CSVs, with explicit blocker statuses. This Python program is external research tooling, not a runtime dependency.
- `examples/re4_character_family_preview.py --profile SLUG` allows carrying the C89 guard into reproducible VTK diagnostic renders.
- `tests/re4_bestiaire_test.c`: naming and mismatched Regenerador/Iron Maiden / armor / giant cases.

## Actual sample renders (PC 2007)
The following pairs of frames 0 and 6 were exported by the C89 multipart geometry exporter and rendered in external VTK with original TGA resources: `em1c` Garrador, `em1d` armored Garrador, `em2b` Gigante A (2 PMDs, reviewed ancestry), `em2f` Del Lago, `em25` parasite, `em22` infected dog, `em21` dog, `em24` snake, `em28` chicken. All 9 test invocations completed, demonstrating data decode, not complete game-like playback. The audit report is generated from previously verified v3.9 PMD weights and v3.2 indexed FCV dataset. It does not independently revalidate all animation frames.

## Open blockers
- Bella Sisters: archives `em42/em45/em46` are animation-only in this supplied `xfile.dat` index. Their specific body/face/chain-saw assembly remains unverified. Do not remap a random `em16` head on them.
- Plaga A/B and bulls: no separate, verified PMD family identified in this corpus; Plaga `em25` is a parasite family and should not be assumed to cover every A/B/C attack form.
- Island soldier `em1e`: FCV exists but most master-PMD tests had no matching rotation channels (alternatives may be parts rather than masters).
- Armadura `em3c`: original primary PMD was incompatible with the sampled motion; a different 00a piece previously had FK compatibility but an authentic animated full-part assembly remains pending.
- Crow `em23` and bat `em29` PMDs exist, FCV exist, but primary model FK matching fails; identify their special channels before playback.
- Police `pl07`: original PMDs present, no local matching FCV package in supplied archive; cutscene motion may be elsewhere.
- J.J. identifiers in different modding lists have ambiguous family/scene semantics; `em14` is a candidate, not a unique guaranteed canonical mapping.
- Morphs, native IK, per-frame bounds continuity, transparency, part-variant state machine, weapon attachments, facial expressions, Win32 runtime integration all incomplete.

## Reproducing checks
```
make clean && make -j4 all && make check
./re4_bestiaire_catalog --all
./re4_bestiaire_catalog --part iron-maiden em3610.pmd # ACCEPT
./re4_bestiaire_catalog --part iron-maiden em3600.pmd # REJECT (exit 2)
./re4_character_clip em3600.pmd anim.fcv 0 6 3 out --creature-profile=iron-maiden # rejects prior to reading files
python examples/re4_bestiary_audit.py --weights /path/all_pmd_weights.csv --families /path/family_coverage.csv --out out
```

### Sources of naming claims (not source code copied)
- https://jaderlink.blogspot.com/2025/10/re4-2007-modding-tutorialsguide-index.html
- https://jaderlink.blogspot.com/2025/01/re4-descricao-dos-arquivos.html
- https://github.com/emoose/re4-research/issues/9
- Original `xfile.dat` and `em.dat` supplied in this conversation, processed by v3.9 native readers.
