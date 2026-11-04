# RE4 Classic Codec v4.4: external mod intake and explicit rigid attachments

## Design contract
- **RE4 PC 2007 / Sourcenext:** `PMD` model, `TGA` texture, `FCV` animation. The source CLI validates standalone extracted mod files, rather than assuming the base-game `emXX/plXX` IDs. It does **not** patch or repack GCA archives.
- **RE4 Ultimate HD:** `BIN`, `TPL` and DDS/TGA, sometimes packaged inside `UDAS`, `LFS`, or image `PACK` data. The `bin-uhd` intake requires *loose files*; the BIN structure is probed, but assembly and animation compatibility are **NOT** certified. No automatic cross-version PMD/BIN conversion is implemented.
- **Unmodified canonical assets vs custom mods:** A syntactically valid mod manifest is not permission to use animations from another skeleton. Clip channel compatibility and per-part joint linking must be checked independently.

## New APIs
- `re4_mod_parse(span, &package)` reads a caller-supplied `R4MOD1` text manifest, using fixed-size arrays and no heap or OS/file access. Rejects directory traversal, absolute paths, duplicate normalized filenames, unknown entries, mixed extensions and excess entries.
- `re4_character_rigid_anchor(master, part, master_bone_index, &links)` fills a part-to-master motion mapping in which every part bone gets the **same world-space delta**. This preserves the part bind pose and enables experimenting with independently rigged accessories, tails or armor pieces. The identity of the anchor must be *explicitly verified by the operator*.

## Example manifest: RE4 PC 2007
```
R4MOD1
format=pmd2007
master=models/custom_character.pmd
part=models/custom_hair.pmd
part=models/custom_left_hand.pmd
clip=motion/idle.fcv
texture=models/custom_face.tga
```

Run from the directory containing `re4_mod_probe`:
```
./re4_mod_probe /my/mod/package.txt /my/mod
```
`SUMMARY,STRUCTURALLY_READABLE_NOT_CERTIFIED,issues=0` indicates that the listed files exist and their supported structures were read. It does **not** establish complete skinning, head/hair montage, UV fidelity, semantic correctness of action clips, or IK equivalence.

## Optional explicit rigid anchor (debugging only)
```
./re4_character_clip master.pmd clip.fcv 0 6 3 out \
  separate_piece.pmd --strict-parts --rigid-anchor=1:25
```
Here `1` is the 1-based part index (the first part after the master) and `25` is a zero-based master skeleton joint index; this MUST be chosen after inspecting the master PMD. Avoid using this for a skinned limb if part-local articulation is required. For Armadura A, the correct anchor is **not yet identified** and no automatic mapping is claimed.

## Known non-goals and limitations
- Not a converter for arbitrary OBJ/FBX/GLB/glTF formats. Mods must already contain a supported RE4-native PMD or BIN. A separate importer/exporter would be required for arbitrary meshes.
- The manifest checker reads loose files. It does not unpack mod ZIP/RAR/7z, execute mod scripts, resolve file-system symlink escapes, update game archives, or trust blindly the renderer output.
- `re4_mod_probe` checks PMD parse, part-to-master mapping, initial FCV channel mapping, texture presence. It does not decode every external texture, verify mesh self-intersection, calculate native IK, or certify every animated frame.
- External samples were tested with renamed files from the user-owned PC 2007 corpus; **no independent community-authored model pack has been tested yet**.
- Still pending: head Plagas A/B/C, canonical Bella Sisters, complete native IK, some armors/rigs, all-game every-frame validation, Windows Win32 build.

## QA (v4.4)
- Strict C89 compilation, 27 automated suites including `re4_mod_intake_test` and `re4_rigid_anchor_test`.
- Real PC 2007 renamed-asset smoke test on Ashley: `90` master bones, `51` direct hair joint links, `13` linked FCV rotation channels, `0` unmatched rotation channels. Parsed special channels remain unresolved.
- One-frame rigid-anchor exporter smoke with an explicit master joint and original pose, source assets not distributed.
