# MilkShape 3D 1.8.5 — Technical continuation (v1.7)

## Authoritative references

- Sapper's annotated MS3D 1.8.5 format (corrects original SDK errors): https://sappersblog.blogspot.com/2014/08/milkshape-3d-185-ms3d-file-format.html
- Original-style spec text: https://gist.github.com/sapper-trle/db1dc6670ec0fa733d7d
- Paul Bourke MS3D early-version layout: https://paulbourke.org/dataformats/ms3d/index.html

## Key semantics retained

- File header `MS3D000000`, format version 4, packed little-endian layout.
- Vertex x/y/z and reference bone; triangle has 3 *position indices*, **3 normals**, **3 S** and **3 T** coordinates, smoothing group and group index.
- Group triangle lists determine group/material bindings; materials expose `texture`, `alphamap`, `transparency` and `mode`.
- Animation stores one collection of joints with independent position and Euler-rotation keys, where joint rotation angles are **radians** and order is extrinsic XYZ (= Rz * Ry * Rx for column vectors).
- No on-disk named animation clip list in this format: clips must be defined by the host as time ranges, or parsed from a separate asset manifest if available.
- Optional model-extra subversion 1 stores joint size, transparency mode and alpha reference.
- Transparency modes: 0 simple; 1 alpha reference with depth buffer; 2 triangles sorted in depth for blending.

## New modules and algorithms

### CORDIC SLERP

`ms3d_quat_slerp()` computes the shortest-hemisphere spherical interpolation of normalized quaternions. It applies hemisphere flipping when dot < 0, and calculates:

    theta = acos(dot(a,b))
    blend = (sin((1-t)*theta)/sin(theta))*a +
            (sin(t*theta)/sin(theta))*b

`sine/cosine` use 16-iteration fixed-point CORDIC. Nonnegative inverse cosine uses 18 steps of monotone bisection in [0,pi/2]. On nearly identical orientations (`dot>=65400/65536`) the method falls back to NLERP to avoid division by a tiny sine. The result is renormalized in Q16.16.

This is *genuine SLERP mathematical weighting* in fixed-point approximation; **not bit-identical IEEE double-precision SLERP**, nor proof of parity with MilkShape's native viewport.

### Blending

`ms3d_skin_evaluate_blend()` interpolates two *local* joint poses (as opposed to linearly interpolating already-skinned vertices), then rebuilds global transforms and inverse-bind skin matrices. The vertex UV and material indices remain untouched. Pose sources are time samples of the **same** MS3D model/skeleton; this does not retarget poses across different rigs.

`ms3d_player_blend(player,secondary_ms,weight)` allows manual crossfade. `ms3d_player_crossfade_to_range(player,start,end,duration)` advances two cursors in parallel and switches to the new interval after the fade. The caller decides clip intervals.

### Transparency-policy adapter

`gif_ms3d_render.h/.c` produces renderer state and a triangle submission queue for the 1.8.5 transparency modes. Mode 2 sorts blended triangles back-to-front by **center depth** along supplied camera-forward vector; ties are deterministic. Opaque and alpha-test passes come first.

This does not decode TGA/JPEG/PNG, alpha-composite or draw pixels. Engines still must sample textures and optional alphamaps, configure depth test, alpha-test and color blending, and re-sort when camera or deforming mesh changes. Sorting by triangle centroid is approximate for intersecting transparent triangles.

## Validation run

- GCC strict C89 (`-Werror -pedantic`) unit tests: PASS.
- Clang strict C89 (`-Werror -pedantic`) unit tests: PASS.
- All 157 supplied corpus models: PASS in independent CLI probe; 3 SLERP poses and one blended pose per model with duration >0, preserving 3 UV records per triangle and material indices; 157/157 successes.
- Synthetic SLERP test: 120° z-rotation, 25% step -> ~30° intermediate (unlike NLERP). Antipodal sign, endpoints and fixed-point consistency tested.
- Synthetic crossfade: transition 0–1000ms -> 1000–2000ms over 400ms with new interval activated after the transition.
- Synthetic material policy: simple blending, alpha-reference cutout, sorted translucency order.
- Animated OBJ/MTL visual QA from `zmb` (735 triangles, one texture) and `zmb_hs` (685 triangles, two textures): 6 frames each at 350ms intervals.

## Limitations and honest scope

- Host's true MSYS2 MinGW32 installation was not available to execute here. Existing build script is provided, untested natively.
- Source still uses C89 `long`; native 32-bit MinGW has 32-bit `long`. Linux x86_64 tests alone do not prove every arithmetic edge case on Win32.
- No joint retargeting across different models, additive animation layers, inverse kinematics, GPU shaders, decoded alpha textures or verified visual equivalence with MilkShape's own rendering.
- Smooth shading and OBJ are included, but OBJ snapshots do not store skeletal animation.
- Geometry gaps/mesh seams that are genuinely present in original assets are not automatically repaired. `zmb_hs` is a different/headless variant and not an example of faces removed by the v1.7 exporter.
- Some original models have no animation joints: the player produces unchanged geometry as expected.
