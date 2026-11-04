# v3.9 — Full RE4 PC 2007 PMD weight audit and creature test harness

The entire **user-provided** `xfile.dat` PMD corpus was scanned: 1,789 PMD entries,
3,924,201 vertices. The NEW strict-C89 `re4_pmd_weights` diagnostic reads each
vertex with the project's existing PMD decoder; no copied or embedded game files.

Checks: weights >= 0, weight0+weight1 near one, positive-influence bone slot
within the current node's inverse-bind table, source bone IDs within the model
skeleton, index/triangle bounds, repeated triangle indices and data span failures.

## Verified results
- 1,789/1,789 PMD structures were parseable by the diagnostic.
- No negative or zero-sum weights, invalid positive-influence slots, invalid
  inverse-bind bone IDs or invalid triangle indices found.
- Maximum absolute weight sum error: 1 unit in Q16.16 (~0.000015).
- 3,747,468 vertices belong to skin-weighted nodes; 176,733 belong to rigid
  nodes without a local inverse-bind table.
- 96 extremely large raw coordinates belong to 3 `xfile/sub/map/r51e` PMDs;
  they were not treated as automatically malformed because non-character map
  geometry may use large sentinels.

THIS DOES NOT VERIFY CORRECT DEFORMATION: constraints, inverse bind matrices,
part-to-master mappings, rest poses, IK, UV, face winding, morphs and original
engine transparency/visibility states require further checks.

## Binary tool
```
make re4_pmd_weights
./re4_pmd_weights extracted_model.pmd
```
Output CSV fields are defined in `examples/re4_weight_corpus.py`. Exit status 0
means no malformed weight/slot/index anomalies according to these checks.

## Whole archive user-owned audit
```
python examples/re4_weight_corpus.py --xfile /path/xfile.dat --out reports
```
Two CSVs: `all_pmd_weights.csv` and `families_weights.csv`.

## Character candidates actually previewed with VTK
- Wesker pl0d: adding **pl0d0c.pmd** to the 5-PMD setup gives face geometry;
  prior pl0d0b-only preview had a black/incomplete facial surface. The face and
  accessory meshes may still overlap in animation: game-level state selection
  and depth/alpha diagnostics not yet finished.
- HUNK pl06: 4 PMD, FCV 24, frames 0 and 6.
- Krauser boss (em39): upper body em3900a, lower body em3901, head em3902,
  hands em39lh00/em39rh00; FCV 54, frames 0 and 6.
- Mutated Krauser (em39): body em3910, head em3911, mutated arm em3912,
  right hand em39rh00; FCV 55, frames 0 and 6.
- Dr. Salvador em16: body+head+two hands, FCV 1 frames 0 and 6;
  weapon/attack interaction not loaded.
- Mutated Mendez em35: upper/lower part, FCV 6 frames 0 and 6;
  a detached-looking appendage appears at frame 6 and needs investigation.
- Regenerator and Iron Maiden em36: 8 base PMD each; FCV 34 frames 0 and 6;
  alternate joint cap and damage states not yet assembled.
- Winged Novistador em2d04, FCV 11 frames 0 and 6.
- Verdugo em2c00 body renders FCV 3 frames 0 and 6;
  independent em2c01 tail is rejected by strict multipart bone match, pending
  a native anchor/mapping rather than forced attachment.

**All character combinations are experimental visual candidate manifests**, not
proof that the original game actually chooses them for each combat state.
VTK and Python are external diagnostics; C89 core remains allocator-free and
Q16.16. No Win32 build or actual engine runtime integration was done here.
