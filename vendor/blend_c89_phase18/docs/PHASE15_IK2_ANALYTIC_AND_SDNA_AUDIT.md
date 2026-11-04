# BLEND C89 — Phase 15: two-bone analytic IK + serialized IK audit

## Design decision

This phase does NOT claim to run Blender's full IK solver. Blender uses its own IK solver tree and constraints, involving target object transforms, pole angle, flags, joint locks, influence, stretching, solver-specific settings and dependency graph. Copying the 3D two-link law of cosines alone cannot reproduce all Blender IK rigs.

The project now has two explicit paths:

1. `src/bc_ik2.c` (`include/bc_ik2.h`): independent, standalone 3D analytical IK for *exactly two rigid links*, with a user-specified pole. It emits 2 deformation matrices usable by the existing `--skin-tsv` kernel; it is *not* automatically triggered by `--play-rigs` on Blender IK objects.
2. `src/bc_ik_audit.c`: read-only scan for on-disk `bConstraint.type=3` and its `bKinematicConstraint` payload through SDNA. Reports chain length, flag bits, serialized target/pole pointers and subtarget names. It does not evaluate those constraints or necessarily identify their owning object.

The IK files in Blender 2.79a and 4.5.14 sources supplied by the user were used as structural references: `DNA_constraint_types.h` describes `bKinematicConstraint` with `tar`, `poletar`, `subtarget`, `polesubtarget`, `rootbone`, `flag`, `poleangle`, `weight`, `orientweight`, and IK subtype. Blender source itself is not redistributed or compiled into this package.

## Standalone IK semantics

- Vectors in 3D Q16.16 (32-bit signed int), lengths Q16.16, checked fixed-point square root/divide/dot/cross.
- Pole target defines the bend side; zero-length and colinear poles are rejected.
- Targets outside the 2-link reachable annulus are rejected rather than silently stretched or clamped.
- Stable canonical rest pose: root at input point, bone0 extends along +Y by `length0`, bone1 begins at `(root_x,root_y+length0,root_z)` and extends +Y. Bone roll uses the plane normal from root-target/pole.
- Poses are built with bone local Y pointing toward each solved joint, matrices are row-major with column vectors, and the output deformation palette is `pose * inverse(rest)`.
- Joint coordinates are emitted as a separate TSV for debugging and testing.
- This is a *canonical straight-rest rig* convention; it cannot be assumed for arbitrary Blender armatures and their custom roll, constraints, local spaces, parenting and nonuniform scaling.

Input `goals.tsv` contains precisely four lines in this order:

```
root 0 0 0
target 1 1 0
pole -1 1 0
lengths 1 1
```

Run:

```sh
./blendc89 --ik2-palette goals.tsv palette.tsv joints.tsv
./blendc89 --skin-tsv palette.tsv vertices.tsv skinned_vertices.tsv
./blendc89 --ik-audit some_scene.blend ik_constraints.tsv
```

The output `palette.tsv` is compatible with existing 13-column index + affine 3x4 rows in `--skin-tsv`. Vertex data is index + source x/y/z + number of bone influences + (bone,weight) pairs; see `examples/phase15_ik2_vertices.tsv`. These standalone commands require no `.blend` file.

## Verification executed in this environment

- `make clean all` with `cc -std=c89 -pedantic -Wall -Wextra -Werror -O2`: PASS.
- Full analytic + downstream skin: `(0,2,0)` becomes `(1,1,0)` under a right-angle target; middle segment point `(0,1.5,0)` becomes `(0.5,1,0)`: PASS.
- 75 deterministic random *3D* reachable targets: distance root-to-elbow matches first length and elbow-to-tip second length within 0.005 scene units, tip within 0.0002: PASS.
- Five unreachable/degenerate cases and one malformed input rejected, with no output files: PASS.
- Synthetic IK-constraint SDNA audit across (4,8)-byte pointers and both endian orders: 8 valid scans + 4 broken pointer refusals: PASS.
- `--ik-audit` opens authentic Blender 2.79a and 4.5.14 source-archive startup scenes (which do not necessarily contain IK): PASS.
- Regressions: phase14 constraints, phase13 multirig, phase12 NLA timing, phase11 auto binding, phase10 objects, phase09 Euler: PASS.
- Phase07 genuine-source corpus: 7 `.blend` scenes exported with previous geometrical vertex/face/UV counts: PASS.
- Phase08 compression tests: 4 cases PASS.
- A separate UBSan-instrumented full build was attempted but timed out; not reported as verified.

## Known limitations

- Blender-authored IK playback is still UNSUPPORTED and is deliberately rejected by existing playback guards.
- The analytic solver does not automatically read Blender `bKinematicConstraint` targets/parameters or automatically modify `--play-rigs` bone matrices.
- No arbitrary bone roll/rest axes, IK twist/poleangle, chain length >2, joint rotation limits, stretching, damping, multiple IK solvers, B-Bones or IK/FK blending.
- Clamped unreachable targets are intentionally unsupported.
- Source code authored by this project retains no heap/no float/no double/no long long, but precompiled zlib/zstd Linux static libraries are not guaranteed C89/no heap and Win32 compressed builds remain unverified.
- Source compatibility with all Blender 1.x–5.x generations and faithful animation matching remain unverified.

## Next interoperability step

Build an IK bridge from `Object.pose.bones` and `bKinematicConstraint` to the analytic solver: resolve actual serialized `tar`/`poletar`, bone parent chain, `Bone.arm_mat` and `Bone.length`, and reject unless a compatible two-bone canonical rest configuration is proven. Compare its pose matrices at sampled frames to evaluated Blender outputs before enabling automatic IK playback.
