# RE4 Classic Codec v3.8 — Luis and Wesker multipart fixes

## Source changes

1. Added the specifically observed **pl04lh04__NN** and **pl04rh01__NN** hand-joint aliases. Luis's PMDs contain both a generic `born__` skeleton and a per-hand auxiliary rig. The latter was previously unmapped, leaving hands in their T-pose while the main body moved.
2. Revised the C89 bone-link auditor to accept a **single, precisely recognized pl04 helper + ordinary `born__NN` collision** as a deliberate alias pair. All other duplicate targets remain invalid. `recognized_hand_alias_pairs` counts the narrow exemption. This is not a general duplicate bypass.
3. Corrected the Wesker diagnostic preset `pl0d` to include **pl0d0e.pmd**, which contains the omitted lower-body geometry. The preset's required-parts checks now prevent accidentally omitting those legs again.
4. Updated the Luis preset to use the new matcher. The underlying C89 retargeting and skinning path remains unchanged, so the correction is due to correctly following the auxiliary hand joints, **not an arbitrary mesh offset**.

## Results on user-provided RE4 2007 samples

| Character | PMD selection | FCV frames | Vertices per frame | TGA | VTK actors | Observation |
|---|---|---|---:|---:|---:|---|
| Luis (`pl04`) | `pl0400`, `pl0401`, `pl04lh04`, `pl04rh01` | 0, 6 (FCV 3) | 12,591 | 6 | 50 | Both hands visually return to cuffs on sampled frames. Previously overall X bounds approximately [-8.698, +8.717]; now [-3.172, +3.294] at frame 0. |
| Wesker (`pl0d`) | `pl0d0a`, `pl0d0b`, `pl0d0e`, `pl0dlh00`, `pl0drh00` | 0, 6 (FCV 24) | 15,078 | 9 | 32 | Legs restored; before, the four-PMD preview had only 10,797 vertices and was cut off at the waist. Face and hand variant fidelity unverified. |
| Ada (`pl0b`) | `pl0b0a`, `pl0b0c`, `pl0b0d`, `pl0blh00`, `pl0brh00` | 0, 6 (FCV 24) | see diagnostic | 10 | 58 | Regression passes; hair visible; canonical facial variant still unverified. |
| Krauser (`pl0a`) | `pl0a0a`, `pl0a0e`, `pl0a0b`, `pl0alh00`, `pl0arh00` | 0, 6 (FCV 24) | see diagnostic | 7 | 36 | Regression passes; mutations/IK unverified. |
| Salazar mutated (`em38`) | `em3800`, `em3801` | 0, 6 (FCV 8) | see diagnostic | 10 | 174 | Existing rigid-node experimental path still exports; `--review-ancestry` required; not the human character. |

All images were rendered by VTK from the user-provided game data decoded/exported by the C89 pipeline; they were not synthesized with a generative image model. Two clips' sampled frames do NOT prove global compatibility for a character's entire motion set.

## Validation

- `make clean && make -j4 all && make check`: 21 C89 suites pass under `-std=c89 -Wall -Wextra -pedantic` with no compiler warnings in this build.
- 21 suites also pass under AddressSanitizer + UndefinedBehaviorSanitizer in a separate clean build.
- Real sample `pl04` FCV 3 frame 0 and `pl0d` FCV 24 frame 0 exported under sanitizers without reported errors.
- New unit tests verify the Luis aliases and the narrow duplicate exception, and explicitly reject two helper aliases targeting the same master bone.
- The optional VTK/Python scripts run outside the C89 core; Win32 has not been compiled/tested here.

## Run

```sh
make -j4 all && make check
python3 examples/re4_character_presets.py --list
python3 examples/re4_character_presets.py --preset luis-pl04 --xfile /path/xfile.dat --em /path/em.dat --out /tmp/luis --end 6 --stride 6
python3 examples/re4_character_presets.py --preset wesker-pl0d --xfile /path/xfile.dat --em /path/em.dat --out /tmp/wesker --end 6 --stride 6
```

### Remaining research

- Confirm face/head variant pairing of Wesker; fix potential mouth/face differences.
- Exhaustive FCV timeline sweeps and IK for all named characters.
- Canonical alternate models and morphs of Ada, Krauser and Salazar.
- Actual Win32 integration with the user's runtime.

Private user-owned PMD, FCV, DAT and TGA data are **not** included in the source package.
