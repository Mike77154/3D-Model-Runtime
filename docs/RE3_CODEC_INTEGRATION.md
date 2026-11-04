# RE3 codec runtime integration — v0.28.0 (Native75)

- Single vendor: `vendor/re3_codec_v0_28/`. The old v0.3.0-datfix vendor is removed.
- Strict ISO C89, fixed point; no new heap allocation.
- 15-section RE3 EMD signature used by provider probe; RE1 retains separate provider.
- Body bind/reference vertices use `re3c_reference_body_vertex()`, which applies per-vertex DAT joint ownership and seam blending when available.
- During bound normal animation, `re3c_build_joint_matrices(RE3C_ROOT_MODEL_SPACE)` plus `re3c_animated_body_vertex()` are used. A matching Normal bank A rig is required; invalid skeletons never get guessed mappings.
- Unresolved damage rigs **remain unbound**. No automatic 15-to-16/17 channel injection or prefix overlay; the V028 damage functions are vendored for explicit caller-led experiments.
- Morph frames, part visibility, and damage transitions require game-state signals and remain *explicit* codec facilities; the generic `modelrt_visit()` API does not yet expose state setters.
- `modelrt_visit()` uses an animation preview cadence of 25 Hz (not game-authentic timing). Timeline-only keys fall back to nearest prior resolvable key.
- RE3 textures are represented by CLUT/PAGE metadata with external texture markers; the runtime provider does **not** automatically parse neighboring TIM files.
- Body normals use a joint-matrix approximation for skinned seam vertices; they are not yet interpolated/blended per joint. Original UV bytes are forwarded as normalized 0..1 values.
- Auxiliary mesh continues through the existing rigid-part transform adapter, not V028 DAT skinning.

## Build and test

`make -j4` builds the complete source-only runtime.

`make re3-regression` generates a synthetic EMD fixture in C89 and runs the adapter regression plus V028's synthetic unit suite.

The supplied V028 audit reports cover real RE3 corpora. This runtime package has not independently re-rendered or visually certified all 100 corpus models. Grave Digger damage is unresolved even upstream.
