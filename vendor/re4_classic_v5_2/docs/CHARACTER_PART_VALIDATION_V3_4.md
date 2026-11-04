# RE4 Classic Codec v3.4 — character-part attachment safety

## Why the fix matters

The old `get_id()` parsed exactly two digits from names such as `born__100`.
For skeletons with 100+ joints it could accidentally select `born__10`,
creating visually incorrect attachments despite a successful read.
The v3.4 parser consumes every decimal digit, supports the historical `_`
suffix used in PMD names, bounds numeric IDs, and rejects malformed suffixes.

`re4_character_links` now records 0/1/2 (none/direct/inherited) per joint.
`re4_character_audit()` does not infer anatomy or a model's canonical identity.
It checks for repeated independently named links and whether directly linked
parent/child relationships remain ancestral in the master skeleton.

A reparenting warning is **not automatically a broken file**. Alternate rigs,
IK-driven meshes and costume variants may legitimately use distinct graph
layouts. The `--strict-parts` exporter option rejects them as a conservative
quality-control measure, rather than silently animating them. It is OFF by
default for compatibility with previous manual tests.

## Actual sample verification

- 484 existing potential pairs, in 53 family namespaces.
- 269 `STRUCTURAL_CANDIDATE`, 206 `NO_DIRECT_LINK`, 9 `REVIEW_HIERARCHY`.
- `em1100.pmd` + `em1101.pmd`: 34 direct links, no conflicts, strict export OK.
- `em3800.pmd` + `em3801.pmd`: 19 direct links, four ancestry conflicts,
  strict export correctly exits 2 without writing geometry.
- Static native exporter increased from 4 MiB to 16 MiB input PMDs.
- 17 suites compile and pass with C89 strict flags and ASan/UBSan on Linux.

All 484 pairs came from pre-existing `part_candidates.csv`, created from
`xfile.dat` supplied by the user. There is no claim that the 484 are every
possible combination of PMD resources in RE4. All reports are metadata only.

## Next work

Character-family canonical selection must use original scene/appearance
records, attachment behavior in engine code and/or game-driven tests, not
naming heuristics alone. The visual rigging of the nine hierarchy warnings
should be investigated separately. Additionally, the original native IK,
facial morphs, full FCV/SEQ playback for every model, translucent materials
and game-equivalent animation blending remain to be validated.
