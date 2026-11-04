# hl2modelib 1.0.18 — validation report

## Change
Source 2013 `bone_setup.cpp` guided the interpolation upgrade. New vendor:
`vendors/source/animator/hl2m_quaternion89.c`. Local rotation blending and explicit
additive gestures now use fixed-point shortest-path quaternion SLERP, rather
than normalized linear interpolation of matrix columns. This is NOT Valve bit parity.

## Automated QA
`make check` compiled and ran with `-std=c89 -O2 -Wall -Wextra -Werror -pedantic -Iinclude`: PASS.
`test_quaternion118`: shortest path (+170 to -170), 180 degree rotation, endpoints,
axis rigidity, additive gesture and invalid weight: PASS.

Independent reference using SciPy 1,000 random rotation pairs at t=0.5:
- Frobenius matrix error mean 0.000111681
- Frobenius error max 0.000265301
- No floating-point, heap or 64-bit types in the C89 codec.

## Source model probes
- Hunter beta Source49 `att1`: sequence at cycles 0, 32768 and 49152: 3/3 PASS.
- Hunter beta `walk_all`: 5-cell blend, 3/3 PASS.
- Nemesis Source49 `ft2_walk`: 3/3 PASS.
- Licker RE3R Source49 `att2`: 3/3 PASS.
These probes check samples and bone axes for plausibility; not exact motion quality.

## Skinning/OBJ case
Hunter beta `att1` at cycles 16384, 32768 and 49152 each exported:
- 213429 output vertices
- 71143 triangles
- 0 missing bones

## Limitations
Licker tongue translation outliers, Source behavior of additive layers and
complete pose parameter remapping remain unsolved. No direct in-engine Source
comparison has been run. Images in this package are diagnostic gray material
renders of the real OBJ meshes, not original texture/shader parity checks.
