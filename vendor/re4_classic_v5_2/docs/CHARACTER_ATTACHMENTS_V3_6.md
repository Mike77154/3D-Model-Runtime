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

