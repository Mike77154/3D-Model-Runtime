# Phase 16 — Bounded serialized Blender IK2 → multirig playback

**Status:** Implemented and tested on synthetic `.blend` files; native Blender-authored IK playback not validated.

## Concrete new integration

The Phase 15 `bc_ik2_solve()` analytic 3D fixed-point solver is now called by `--play-rigs` when a compatible `bConstraint`/`bKinematicConstraint` is found in a pose channel. This is no longer exclusively a standalone `--ik2-palette` tool.

- `bc_constraints.c`: serialized IK constraint detection, strict option validation, target/pole resolution through typed SDNA pointers, static Object origin checks, conversion from world space to Armature Object space, per-rig IK goal storage.
- `bc_playback_tsv.c`: bone lengths from actual `Bone.arm_head` and `Bone.arm_tail`, chain parent validation, pose solution for parent and child, root palette recomputation, existing vertex-group skinning and OBJ output.
- `include/bc.h`: narrow API between these modules.
- `tests/test_phase16_ik_playback.py`: end-to-end data-driven cases for pointer width, endian, multiple rigs, armature world translation and fail-closed invalid input. The shared `test_phase13_multirig.py` synthetic fixture generator is extended to make the serialized IK scene; previous constraints and multirig tests still pass.

### Accepted narrow subset

1. Exactly two bones in the IK chain (`rootbone = 2`), with the constraint on the child. The parent is the root bone of the armature (no higher bone parent), and the child must reference it in its serialized `Bone.childbase` hierarchy.
2. Rest matrices are in the **canonical +Y bone orientation**, with no rest rotation, each bone having a positive Y length. Child rest head must coincide with the first bone's tail. This is crucial: arbitrary Blender bone roll and rest orientations are not yet mapped to the analytic solver.
3. `bKinematicConstraint.type = 0` (Copy Pose), `flag = 33` (tip + position), `rootbone = 2`, `max_rootbone = 0`, `weight = 1`, `orientweight = 0`, `poleangle = 0`, `mode = 0`, no targetless IK, stretching or axis locks.
4. Separate target Object and pole Object, both **unparented and unanimated**, without Object constraints or delta location, and without bone subtargets; target positions come from the saved `Object.loc` (not the evaluated dependency graph). Root translation from a compatible active Action is supported; additional child channels and root rotation/scale are rejected.
5. Constraints use pose-space settings already understood by the Phase 14 kernel, influence 100%, head/tail blend zero and no additional active constraints affecting this two-bone chain.
6. One Armature modifier per linked mesh under the established Phase 13 multirig rules. World-space target and pole positions are transformed by the inverse of the Armature Object affine transform. The mesh/armature conversion uses the existing Phase 10 transform pipeline.

The implementation rejects unsupported or malformed IK requests instead of silently applying Blender-like but incorrect motion. The solution uses the original *analytic* IK2 kernel, **not Blender's iterative IK solver**; it does not guarantee the same pole convention, bone roll, limits or final pose Blender would calculate.

### Reproduce included synthetic test

```sh
make clean all
python3 tests/test_phase16_ik_playback.py
./blendc89 --ik-audit examples/phase16_two_rigs_ik_SYNTHETIC.blend examples/phase16_test_audit.tsv
./blendc89 --play-rigs examples/phase16_two_rigs_ik_SYNTHETIC.blend 5 examples/phase16_test
```

In the test, the first rig's root is animated to `(0, -3, 0)` at frame 5, the saved target Object is `(1, -2, 0)`, and its pole Object is `(0, -1, 0)`. The IK child deforms the mesh vertex originally at `(0, 2, 0)` into `(1, -2, 0)`; the second rig with the same bone name `Root` is independently deformed to the expected `(5, 0, 0)` vertex. Additional tests move the armature and mesh by +10 world-X and verify unchanged mesh-local results.

### Validation executed in this session

- Build with `-std=c89 -pedantic -Wall -Wextra -Werror -O2` succeeded.
- A second **source-only uncompressed variant** compiled in strict C89 without linking zlib or Zstandard, and reproduced the basic IK scene.
- Phase 16: **12 positive** playback/audit/space tests + **40 negative** fail-closed cases, across 32- and 64-bit serialized pointers and little-/big-endian bytes.
- Phase 13 multirig and Phase 14 Copy constraints tests passed.
- Phase 15 analytic IK, Phase 15 audit, Phase 09 Euler tests passed.
- Seven genuine `.blend` geometry OBJ exports from Blender source distributions passed with expected counts. These are geometry/regression tests, **not genuine IK playback comparisons**.
- Full logs: `docs/PHASE16_TEST_RUN.log`.

### Remaining work

True Blender iterative IK parity, arbitrary bone roll and rest axes, pole angles, target bones and moving/parented targets, IK constraints with NLA/driver effects, extra chain links, stretching and limits, B-Bones, stacked constraints, depsgraph evaluation, Blender 1.x–5.x real-asset playback validation. Compressed-input support uses bundled **Linux x86-64 third-party static** zlib and zstd libraries; they have not been converted to heap-free C89. The Win32 build script is uncompressed-only and has not been cross-compiled in this session.

### Licensing

Blender's distribution source was consulted to understand serialized DNA fields and IK flag constants; no Blender GPL implementation files were copied into this codec. Original project implementation retains its own LICENSE; vendored compression libraries retain their corresponding separate licenses.
