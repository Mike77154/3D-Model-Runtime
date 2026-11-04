# SKA skeletal animation -- new challenge implemented (v0.3)

## Scope

`ss89skin.c` implements an independent C89 / integer-only binary decoder and
CPU linear-blend-skinning runtime for the *Serious Engine 1* skeletal branch:

| Actual signature | Typical resource | This revision |
|---|---|---|
| `MESH` 11/12 | `.bm` | Vertex positions, first UV set, triangles, named weight maps |
| `SKEL` 6 | `.skl` | Named bones, parent graph, absolute bind matrices, relative rest pose |
| `ANIM` 14 | `.ba` | Named clips, sparse position/rotation keys, frame interpolation |

**`SKA` names the engine subsystem; it is not a single unified `SKA` file signature.**
`ADAT` / `.ani` and `MDAT` / `.mdl` are handled by the pre-existing tools.

The modules accept separate input files; `ss89gro` can extract them from a `.gro`.

## Usage

```sh
./build_msys2_mingw32.sh
./bin/ss89pose.exe monster.bm monster.skl monster.ba --clip 0 --frame 0 --obj rest.obj
./bin/ss89pose.exe monster.bm monster.skl monster.ba --clip 0 --frame 15 --sub 32768 --obj mid.obj
./bin/ss89pose.exe monster.bm monster.skl monster.ba --clip 0 --sequence pose_ --count 30 --loop
```

On Linux use `./build.sh` and executables without `.exe`.

`--sub` is the fractional frame in Q16.16 (0..65535). `--frame` and
`--clip` are zero-based. `--sequence prefix --count N` exports successive
full frame meshes as OBJ (`prefix0000.obj`, `prefix0001.obj` etc.).
`--loop` repeats after the clip's frame count, interpolating across a wrapped
key interval.

## Vendor API

```c
#include "ss89skin.h"
static SS89S_Point point;
/* Paths must be accessible on disk, for instance after unpacking a GRO. */
if (ss89s_load_mesh("monster.bm", 0U) &&
    ss89s_load_skeleton("monster.skl", 0U) &&
    ss89s_load_animation("monster.ba")) {
    if (ss89s_pose(0U, 0U, 32768U, 1)) {
        ss89s_get_vertex(0U, &point);
        /* point.x, point.y, point.z are signed Q16.16. */
    }
}
```

The workspace is static and **non-reentrant**; the next load or pose overwrites
its data. Vertex and triangle accessors are suitable for a presenter adapter.
The runtime is not a renderer; it does not use OpenGL, DirectX or WinGDI.

## Deformation algorithm

1. Resolve names: the mesh's weight-map ID must match the skeleton bone name.
2. Resolve the skeleton parent graph and order it topologically. Reject cycles.
3. Start each pose from bone-local rest positions/quaternions.
4. Evaluate sparse position tracks using linear interpolation, and quaternion
   tracks using shortest-arc normalized linear interpolation (NLERP, **not**
   an exact recreation of original engine SLERP).
5. Compose 3x4 matrices in row-major order: `world = parent_world * local`.
6. Per bone, `skin_matrix = world * inverse(rigid_absolute_bind_matrix)`.
7. For each influenced vertex, accumulate `weight * (skin_matrix * vertex)`.
8. For an unweighted vertex, keep its mesh-rest position. For weights whose sum
   differs from 1, renormalize the accumulated result.

Quaternion file component ordering is **w, x, y, z** (Serious Engine
`Quaternion<Type>`), *not* x,y,z,w. `AnimPos` binary struct occupies 16 bytes;
`AnimRot` occupies 20 bytes; each has a 16-bit frame number plus 2 bytes padding.
Compression flags in the file do **not** mean the rotation payload is compressed:
Serious Engine may compress rotation storage *after reading* the 20-byte keys.

Fixed-point operations use 16-bit limbs and integer divisions; no 64-bit,
heap allocation or floating-point maths in the shipped C modules. Very large
coordinates saturate to signed Q16.16 limits. This is an approximation of
original engine precision and interpolation; suitable for runtime validation,
not bit-exact reproduction of native IEEE-754 output.

## Verification and honest limits

- Synthetic triangle / two-bone fixture: pose(0) = (2,0,0), pose(1) =
  (1,1,0), halfway = (1.707,0.707,0) for its child-controlled vertex.
- Two weight maps (root + child) influence a blended vertex as expected.
- Tested reversed parent serialization, loop wrap, cyclic rig and truncated ANIM.
- Genuine *Lamp.bm* and *Axis.bm* from the supplied SE1 archives were opened
  with this module. These original meshes have **zero weight maps**.
- The supplied GRO had no authentic matching `.skl` + `.ba` + skinned `.bm`
  triplet, so actual creature skeleton animation is **NOT VERIFIED**.

Not yet implemented: morph target animation, skinned normals, shader settings,
blend trees, original engine's exact SLERP, `.smc` scene assembly, `.wld` loading,
real-time windowed animation preview. A standalone SKA rig is not a full game
animation system, and this does not animate the classic `MDAT` mesh (separate
vertex-frame path from v0.2).

## Format reference pointers (do not vendor engine source)

- `Sources/Engine/Ska/Mesh.cpp` MESH serialization
- `Sources/Engine/Ska/Skeleton.cpp` SKEL serialization
- `Sources/Engine/Ska/AnimSet.cpp` ANIM serialization
- `Sources/Engine/Ska/RMRender.cpp` reference bone hierarchy / skinning
- `Sources/Engine/Math/Quaternion.h` quaternion memory layout

Official upstream: https://github.com/Croteam-official/Serious-Engine
Croteam's original engine is GPLv2. The files in this archive are newly authored
code and illustrative independently generated fixture data, not bundled game
assets. Assess all licenses before incorporating code or game files into products.
