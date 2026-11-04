# Real MDL controller regression — Resident Evil Source49

Compiled: `-std=c89 -O2 -Wall -Wextra -Werror -pedantic -Iinclude`

| Model | Sequence index | Sequence | Physical Q0.75 | Identical matrix entries vs physical API | Changed local bone transforms Q0.25 versus Q0.75 |
|---|---:|---|---|---|---:|
| `nemesis_ft1_v2.mdl` | 33 | `aim_pitch` | +45° | Yes | 3 |
| `nemesis_ft1_v2.mdl` | 34 | `aim_yaw` | +45° | Yes | 2 |
| `hunterb.mdl` | 30 | `walk_all` | +90° | Yes | 32 |
| `g1.mdl` | 11 | `fast_walk`, apply_layers=1 | none | Yes, compared with v1.0.20 layered API | 0 |

Measured from original MDL files provided by the user. The `.mdl` corpus itself
is not copied into the library ZIP. Re-run the included
`hl2m_pose_controller_probe121` against those files to verify.

The nonzero changed-bone count proves controller-driven pose variations,
not that all original engine animation features are reproduced. All
registered C89 regression targets passed.
