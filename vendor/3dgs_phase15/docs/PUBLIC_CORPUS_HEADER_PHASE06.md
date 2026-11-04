# Public corpus / MDL7 structure observation

File: `PhosphoricAcid_MDl7.mdl` from upstream `assimp/assimp`, test/models/MDL/MDL7 (3DGS A7).
Read-only GitHub API metadata inspection, **not** a locally executed native model test.

| Group | Offset | Byte count | Skins | Vertices | Triangles | Frames | Deformers |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 0 | 48 | 3864 | 3 | 62 | 120 | 0 | 0 |
| 1 | 3912 | 3576 | 0 | 62 | 120 | 0 | 0 |
| 2 | 7488 | 3576 | 0 | 62 | 120 | 0 | 0 |
| 3 | 11064 | 3576 | 0 | 62 | 120 | 0 | 0 |
| 4 | 14640 | 3576 | 0 | 62 | 120 | 0 | 0 |
| 5 | 18216 | 3576 | 0 | 62 | 120 | 0 | 0 |
| 6 | 21792 | 3576 | 0 | 62 | 120 | 0 | 0 |
| 7 | 25368 | 3576 | 0 | 62 | 120 | 0 | 0 |

Offset end: 28,944. Bone count: 0. Main vertex stride: 26. Triangle stride: 16.

The local synthetic test `tests/fixtures/mdl7_eight_groups_26.mdl` is
constructed with these counts and new geometry; it does not copy source bytes.

To test genuine files on your machine, run `scripts/verify_public_corpus.sh`
after obtaining the actual corpus files in accordance with their license.
