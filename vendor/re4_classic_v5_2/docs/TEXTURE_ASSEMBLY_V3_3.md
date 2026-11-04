# v3.3: Texture + multipart validation

Three **different** layers are tested independently:

1. **Named texture reference:** a material in a PMD advertises a TGA basename.
2. **Archive resolution:** a TGA of that name exists in the PMD's GCA folder;
   same-folder has precedence over global matches to avoid collisions across
   costumes, enemy families and shared objects.
3. **Actual image decode:** Pillow can decode every indexed TGA; VTK can use a
   subset in a complete multipart OBJ, exported by the C89 engine.

These results are not a UV quality proof, not visual verification of every
texture, and not canonical character part selection.

When scanning candidate pieces, only exact `family01.pmd`, `familylh00.pmd`,
`familyrh00.pmd` and `lh00.pmd` / `rh00.pmd` filenames are considered for the
corresponding role; `*lo.pmd` and variant suffixes are excluded. The existing
skeleton-link probe must report at least one direct matching bone. Status
`SINGLE_CANDIDATE_NOT_CANON` means a plausible default, not evidence that
this game actually attaches it in every animation.

To reproduce the VTK PNG previews the scripts require the original user files,
optional VTK and Pillow. Python code controls extraction and visualization;
actual PMD vertex deformation and FK come from the native C89 binaries.
