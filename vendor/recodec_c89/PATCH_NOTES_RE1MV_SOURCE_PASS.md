# Patch notes: RE1MV source pass

- Added RE1MV-verified EMR header interpretation to `tools/emd_textured_viewer.c`.
- Fixed EMR frame stride: use `EMR+6 FrameSize`, not `EMR+0 PartListOffset`.
- Fixed EMR animation offset: use `EMR+2 AnimationOffset`.
- Fixed bind/root translation scaling using RE1MV `Calculate_Pos()`.
- Fixed RE1 bind triplet axis order (`Z,Y,X`) from RE1MV `Load_Ans()`.
- Fixed root motion axis order (`glTranslatef(Z,Y,X)`).
- Remapped angles to match RE1MV's OpenGL calls: `X=-Z`, `Y=-Y`, `Z=-X` for the existing software matrix path.
- Kept the two-bank RE1 EMD split: `EMR_A/EDD_A` and `EMR_B/EDD_B`.
- Validated on `EM1111(5).EMD`, Bank B Clip 0, frames 0/5/10/15/20/25/30.
