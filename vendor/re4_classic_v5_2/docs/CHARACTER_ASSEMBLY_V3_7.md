# v3.7 – PC 2007 character assembly diagnostic

Based on real, user-supplied `xfile.dat` and `em.dat`, *not* a reference-engine gold standard.

## Verified engineering changes

- Added `pl0dlh00__NN` and `pl0drh00__NN` aliases to master `born__NN` for Wesker hands.
- Added `em39lh00__NN` and `em39rh00__NN` aliases for Krauser pl0a (hand assets reuse em39 naming).
- C89 synthetic regression verifies those aliases and rejects invalid names.
- New `re4_skel_skin_rigid_node` accepts PMD nodes without a local bone table *only* when they have one unit-weight vertex and zero slot indices; transforms with animated world matrix times inverse-rest matrix of `node.skeleton_index`. Does not change two-influence skinning.
- Real `em3800.pmd` contains 59 zero-table nodes and 79,959 rigid-node vertices, first failing node 66 (1776 vertices). The new path permits a two-part export with a `--review-ancestry` note. This is **mutated Salazar**, not human Salazar. World-space rigid interpretation remains a fidelity hypothesis, not proven exact.

## Tested multipart variants

| Family | Configuration | Results | Caveat |
|---|---|---|---|
| Ada `pl0b` | `pl0b0a`, `pl0b0c`, `pl0b0d`, `pl0blh00`, `pl0brh00` | five PMD, 10 TGA, 58 VTK actors; frames 0,6,12,18,24 | `pl0b0d` is the missing hair. Still candidate face variant, IK not native. |
| Krauser `pl0a` | `pl0a0a`, `pl0a0e`, `pl0a0b`, `pl0alh00`, `pl0arh00` | five PMD, 7 TGA, 36 VTK actors; frames 0,6,12,18 | head/torso/legs/hands visible; mutated arm `em39` not included. |
| Luis `pl04` | `pl0400`, `pl0401`, `pl04lh04`, `pl04rh01` | 4 PMD, 6 TGA, 50 VTK actors; frames 0,6 | **INCOMPLETE**: the hands float away from sleeves despite structurally accepted links. |
| Wesker `pl0d` | `pl0d0a`, `pl0d0b`, `pl0dlh00`, `pl0drh00` | 4 PMD, 9 TGA, 27 VTK actors; frames 0,6 | **INCOMPLETE**: hand joints now link, but chosen `pl0d0a` is upper-body-only. `pl0d0h` is another head variant with different bind-space position. |
| Mutated Salazar `em38` | `em3800`, `em3801` | 2 PMD, 10 TGA, 174 VTK actors; frames 0,6 | `--review-ancestry` 4 conflicts; rigid node transform fidelity not proven; no human Salazar model claimed. |

**FCV scope:** these are selected frames in chosen FCV entries; totals of FCV entries do not establish animation quality. Source character names cross-referenced with JADERLINK's RE4 2007 public file list, verified against names in uploaded GCA archives.

## How to run presets

```sh
make all && make check
python3 examples/re4_character_presets.py --list
python3 examples/re4_character_presets.py --preset ada-pl0b \
 --xfile /path/xfile.dat --em /path/em.dat --out /tmp/ada \
 --end 24 --stride 6
```

For incomplete or review-only examples, supply `--allow-incomplete`. This option is diagnostic and deliberately does not certify the result.

## Limits

- No forced interpretation of alternate face/hair pieces whose skeleton identity does not match.
- No independent game-engine render comparison, original IK, procedural hair, or perfect skinning guarantee.
- C89 core has no external dependencies/heap/float; Python+VTK used only for diagnostics.
- Windows/Win32 build remains untested.
- Proprietary PMD, TGA, FCV, DAT are never included in redistributed source zip.
