# R8 MDL7 frame poses

`mdl7_rig.c:gs7_frames` now applies the selected frame's XYZ vertex overrides to a static Q16.16 position array.
The OBJ is generated after those overrides; deformer records are still read
after frames as assumed in R7. Bone-matrix application is pending authentic validation.
