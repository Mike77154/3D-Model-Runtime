# hl2modelib v1.0.24 —  recursive includes and virtual autolayers

**New optional C89 APIs:** `hl2m_virtual_build_recursive124()` and
`hl2m_master_virtual_sequence_layers124()`. A caller supplies a root MDL and
up to three uniquely named included MDLs, including their MDL/ANI bytes.
The resolver visits indirect `$includemodel` declarations recursively, merges
catalogs in depth-first order, shares diamond dependencies once, and rejects
missing, orphaned, cyclic or duplicated inputs without partially overwriting
caller outputs. Sequence `STUDIO_OVERRIDE` and `iRelativeSeq` are remapped.

The new opt-in animator can follow autolayers into another MDL group, inherit
weights, project per-bone masks to the base skeleton, and convert `STUDIO_AL_POSE`
parameters via the target group's shared pose table. Unsupported WORLD/LOCAL
modes, invalid DELTA and incompatible skeletons fail explicitly.
**No complete Valve Source runtime / IK equivalence is claimed.**

Independent modules:
`vendors/source/animator/hl2m_virtualmodel124.c` and
`vendors/source/animator/hl2m_virtual_layers124.c`.
All previous codec vendors and master APIs are retained. Constraints:
ISO C89, 32-bit Q16.16, no malloc/free, no float/double or int64.

Run `make check CFLAGS="-std=c89 -O2 -Wall -Wextra -Werror -pedantic -Iinclude"`.
Use `make hl2m_virtual_probe124` to test your own included MDLs.
See `docs/SOURCE_VIRTUALMODELS124.md` for exact test scope and usage.

## v1.0.20 — Source automatic sequence layers

Opt-in API: `hl2m_master_sequence_layers120` in `include/hl2m_master89.h`.
Adds bounded recursive Source autolayer accumulation (weights inherited),
physical `STUDIO_AL_POSE` envelopes, parent-cycle inheritance, circular-layer
protection and transactional output. Fixed `posekeyindex` axis-1 stride.
Does not change the v1.0.19 API. See `docs/SOURCE_SDK_AUTOLAYERS_V120.md`.

Build/tests: `make check CFLAGS="-std=c89 -O2 -Wall -Wextra -Werror -pedantic -Iinclude"`.
Real corpus comparison (supply the MDL yourself):
`make hl2m_layer_tree_probe120 && ./hl2m_layer_tree_probe120 birkin_g1.mdl fast_walk`.

# hl2modelib v1.0.19 — C89 Source Studio vendor

**New:** Source 2013-informed additive/POST animations, sequence-local physical pose parameters, per-bone weights and metadata-driven autolayers. The independent vendor `vendors/source/animator/hl2m_sequence_layers89.c` can be called from the `hl2m_master89` API. See `docs/SOURCE_SDK_SEQUENCE_LAYERS_V119.md` for verified corpus tests, limits and the DELTA quaternion repair.

# hl2modelib v1.0.18 — Source-inspired quaternion sequence blending

**New independent C89 vendor** `vendors/source/animator/hl2m_quaternion89.c`.
Sequence interpolation, blend grids and caller-weighted additive gesture layers
now interpolate local bone rotations via **shortest-arc quaternion SLERP**, instead
of linearly mixing the 3x3 matrix columns and applying Gram-Schmidt.
Translation still interpolates linearly; native MDL/VVD/VTX/VTF and master entrypoints remain intact.

- ISO C89, Q16.16, signed 32-bit arithmetic only, no heap, floats or int64.
- Public entrypoint `hl2m_quaternion_blend_matrix89(a, b, weight, out)`.
- `HL2M_VERSION_PATCH=18`, `HL2M_MASTER_ABI89=0x0118`.
- Rebuild all client binaries; header and ABI-version update.
- `make check-quaternion118` and `make check`.
- Numeric test: 1,000 random rotation pairs compared against SciPy SLERP,
  Frobenius matrix error mean 0.000112, max 0.000265 (external QA only).
- Tested sequences: Hunter beta `att1`, `walk_all`, Nemesis `ft2_walk`,
  Licker `att2`, with 3 fractional samples per sequence and no format errors.

**Limitations:** No proof of full Source playback parity; unmodified outlier source
translations in Licker attacks, incomplete IK/layer scheduling; not a fix for
broken input animation tracks. See `docs/SOURCE_QUATERNION_SLERP_V118.md`.

# hl2modelib 1.0.17 — Seven Resident Evil Workshop packages

See `docs/RE_WORKSHOP_7_PACK_CORPUS_V117.md` for the actual new limits, bone-ID fix, test corpus and known limitations. Source files are not bundled. All old master entry points remain present; recompile dependent engines for ABI 0x0117.

# hl2modelib v1.0.16 — Source SDK sequences and gestures

Adds `vendors/source/animator/hl2m_sequence_eval89.c`, fractional sequence cycles, 1D/2D blend-grid selection, and explicit caller-weighted additive gesture composition. See `docs/SOURCE_SDK_SEQUENCE_PARITY_v116.md` for exact limitations. Old vendor/module APIs remain available. **This is not full Valve engine playback.**

# hl2modelib Source 1.0.15 — native animation diagnostics

**IMPORTANT:** Authentic Source playback remains the default. The **optional** guard mode produces salvaged previews of Source assets with outlier local translations; it does not prove animation parity. See `docs/SOURCE_SDK_ANIM_OUTLIERS_v115.md` for measurements and reproduction.

Quick check: `make CFLAGS="-std=c89 -O2 -Wall -Wextra -Werror -pedantic -Iinclude" check`

`make hl2m_anim_guard_probe115 hl2m_export_animated_obj89`

`./hl2m_anim_guard_probe115 licker.mdl 1 74 512`

`./hl2m_export_animated_obj89 licker.mdl licker.vvd licker.dx90.vtx 1 74 recovered.obj --guard-outliers 512`

The threshold is a **preview policy**, not a Valve constant. Never silently enable the guard in a game runtime. C89 only, caller-owned buffers. Older master and vendor interfaces remain present.

## v1.0.14: animaciones Resident Evil y VTF

Nueva API del master: `hl2m_master_anim_external89` acepta los bytes MDL+ANI para motion banks externos. Los MDL seccionados funcionan también con `hl2m_master_anim89`. Inspecciona `docs/RESIDENT_SOURCE_ANIMATION_MATERIAL_AUDIT_v114.md`; las pruebas no garantizan movimiento continuo idéntico a Valve.

`make check` compila y ejecuta también `test_anim_sections114`.

# hl2modelib 1.0.13 — HOD2 Source 48 verification

See `docs/HOD2_SOURCE48_CORPUS_v113.md` for the corpus regression, material limitations and tools. The C89 decoder and ABI are unchanged from the v1.0.12 Licker/SMD experimental build.

# hl2modelib v1.0.12 — Resident Evil Source corpus compatibility

**Latest verified stress-test:** two user-supplied ZIPs totaling 100 MDLs. All 91 complete MDL/VVD/VTX sets assembled in C89; 9 extra MDLs have no complete mesh companions (animation banks). 438 VMT and 554 VTF files passed their metadata parser checks. Six models were independently exported as OBJ/MTL and previewed without material textures. See `docs/RESIDENT_CORPUS_STRESS112.md` and `CHANGELOG.md`.

**ABI:** `HL2M_MASTER_ABI89=0x0112`. Rebuild clients: compile-time limits enlarge C structs. No `float`, `double`, `int64`, `malloc` or `free` were added to the codec.

The mesh, animation, IK, physics, material and texture vendors remain available. This release does not claim pixel shader parity or correct playback of every motion. `make check` plus `make check-corpus112 check-vtf-uncompressed112 check-vvd-fixup112` run strict C89 checks.

# hl2modelib v1.0.11 — Source UV / coat mapping audit

Additive release; compatible with v1.0.10 and strictly ISO C89. Added
`vendors/source/texturizer/hl2m_uv_filter89.c` for Source-style bilinear
filtering in normalized Q16.16 coordinates (top-left origin, optional clamp/repeat).
The new `hl2m_master_smooth_fragment_linear89()` interpolates Source UVs,
normals/tangents and samples the original VTF **per fragment**.

Fixed mesher source lookup to use the SDK's `mstudiomodel_t.vertexindex` and
`tangentsindex` byte offsets instead of assuming all bodygroup models are
contiguous. The T-103 phase 1 had only one model, so that part does not explain
its coat's appearance; using the right offsets matters with alternate bodygroups.

**Does not alter UV islands, force a vertical flip, or generate new geometry.**

See `docs/SOURCE_SDK_UV_TEXTURIZER_v111.md` for a Source-SDK-lineage audit
and full integration instructions.
Source sampler nearest path remains callable and unchanged. Bilinear is
optional. Renderer supplies perspective-correct barycentric weights; current
software preview orthographic and has no mipmapped/trilinear/anisotropic
filtering. Do not interpret it as exact DX9 Valve pixel parity.

# hl2modelib

Current version: **1.0.23**


## Release 1.0.8 — preserved Source RLE facial rotation precision

- Preserves the original IEEE-754 bit patterns of per-bone `rotscale` and
  `posscale` during MDL decode, in addition to existing Q16.16 metadata.
- New integer-only `hl2m_native_scale_rle89()` computes signed RLE sample ×
  original float scale into Q16.16 using a manual two-word product (no int64,
  float, double, malloc or external math libraries).
- Native animator uses unrounded scales for RLE Euler rotation and position
  channels. Corrects lost jaw/lip/eyebrow movement for T-103 phase 1.
- `STUDIO_ANIM_RAWROT2` quaternion path is **preserved** and verified on
  `Attack_Stand_2H`. It was not the cause of this precision defect.
- Added `test_facial_precision108` (11 arithmetic cases) and real T-103 facial
  matrix probe. Offline before/after OBJ exports confirm the mouth opens.
- **Recompile all clients**: `hl2m_mdl_bone` contains new raw-scale fields, so
  binary struct layout changes. Source APIs remain present, but this is an ABI
  version update (`HL2M_MASTER_ABI89 == 0x0108`).
- This patch fixes recovered bone motion, not full Source sequence blending,
  IK, frame interpolation, facial morph flexes or GPU shader parity.

See `docs/SOURCE_SDK_FACIAL_PRECISION_v1_0_8.md` for verification.

## Release 1.0.6 — SDK-based two-bone IK and character contact

Adds the independent `hl2m_ik_2bone89.c` and `hl2m_character89.c` vendors,
new master APIs for joint IK, persistent foot contact and kinematic grounding,
and optional real-T103 IK previews. Existing 1.0.5 functions remain available.
Validated both leg chains across all 3 phases of native Walk_F.
See `docs/SOURCE_SDK_PHYSICS_IK_V1_0_6.md` for algorithm, integration and
limitations. This does NOT reproduce Source's entire runtime/animation graph.

## Release 1.0.5 — physics, IK, shaderer, SMD/VTA/QC vendors

Three new separately compiled modules live in `vendors/source/physics`,
`vendors/source/ik` and `vendors/source/shaderer`. VMT texturization and
software fragment shading are now separate code units. The SMD, VTA and QC
parsers/writers have been extracted into `vendors/source/formats/` alongside
ANI. Four linkable format facades are called through the master codec.

Added master APIs for PHY metadata, ground/AABB physics, foot-contact root
correction and software fragment shading; strict C89 master integration tests.
There is **no true Source IK or VPhysics parity yet**. See
`docs/MASTER_VENDOR_V1_0_5.md` for the exact boundaries and API.

## Release 1.0.4 — vendored subformats and master codec

Source-specific MDL/VVD/VTX/VMT/PHY/ANI parser implementations moved into
vendor `.inc` files. Native VTF codec, animator, texturizer and the new master
codec compile as independent `.c` objects. The mesher and skinner are in
separate vendored `.inc` files included from the legacy compilation unit to
preserve their internal C89 helper dependencies. SMD/VTA/QC stay in the
legacy core but are dispatched by `hl2m_master_decode89`.

New MDL movement-curve evaluator derived from the SDK (`Studio_AnimPosition`),
plus correction for `STUDIO_ANIM_DELTA` zero/identity channel behavior.
The animation OBJ exporter now accepts an optional `--entity-motion` flag.
**Foot IK, frame interpolation, animation blending and full Source GPU parity
are not implemented.** See **`docs/MODULAR_V1_0_4.md`** for limitations,
directory layout, C89 build commands and API usage.

## Release 1.0.3 — Tyrant native animation and material shading

Native Source 48/49 animation decoding (`mstudioanim`, per-bone compressed channels,
Quaternion48/64, original `poseToBone` bind matrices), new animated CPU-skin OBJ
exporter, renderer-neutral VMT FX / fixed-point fragment shading supporting
normal maps, Phong, rim, detail, environment and alpha-test, and a real-corpus
VMT FX inspector. All previous decoders remain in the build.

See **`docs/T103_ANIM_SHADER_v1_0_3.md`** for exact API call order, C89 build
commands, verified T-103 corpus samples, shader preview scope and unsupported
native Source runtime features.

Clean-room C89 codec foundation for Source Engine / Half-Life 2 model assets.

Supported decode/import paths:

- `.mdl` Studio Model metadata: header, bones, textures, cdtexture paths, bodyparts, models, meshes, hitbox sets, local animation descriptors, sequence descriptors and animblock table metadata.
- `.vvd` vertex data: vertex header, fixups, weights, bones, position, normal, uv, tangents.
- `.vtx` optimized draw topology: bodyparts, models, LODs, meshes, stripgroups, strips, index table, vertex remap table, per-LOD material replacement lists.
- `.smd` ASCII reference meshes/animation: nodes, skeleton frames, triangles, weights.
- `.vmt` Source material KeyValues: shader and flattened parameter tree.
- `.qc` Source model compile recipe: command IR plus structured bodies, sequences, LODs, attachments, multiple hitboxes, definebones, poseparameters, collision sources, material replacements, flex controllers/rules, sequence events and IK chains/rules.
- `.phy` Source collision container metadata: header/checksum/solid count plus compact VPHY or legacy solid headers.
- `.ani` animation block companion metadata: IDST-style header and animblock byte ranges, with optional scan using an already decoded MDL.
- `.vta` vertex animation / flex target text data into fixed-capacity flex records.

Engine-facing paths:

- `hl2m_build_engine_model()` joins decoded `MDL + VVD + VTX` into fixed-point draw batches and resolves VTX LOD material replacements into draw material names.
- `hl2m_build_engine_model_from_smd()` builds draw batches directly from a reference SMD.
- `hl2m_engine_model_bounds()` computes fixed-point bounds.
- `hl2m_engine_model_validate()` validates draw/index ranges.
- `hl2m_engine_model_write_obj()` writes a debug OBJ text buffer without dynamic allocation.
- `hl2m_vmt_to_material_desc()` extracts a compact material descriptor for renderer binding.
- `hl2m_compile_qc_smd_to_binary()` emits a coherent MDL/VVD/VTX/PHY/ANI bundle from QC + reference SMD + optional animation SMD + optional VTA.
- `hl2m_make_compile_options_from_qc()` derives compiler options from QC commands, including model name, body name, material surface, QC LODs and collision mode hints.
- `hl2m_validate_compile_inputs()` checks SMD/QC/VTA/animation consistency before writing binary data.
- `hl2m_validate_binary_bundle()` checks the generated MDL/VVD/VTX/PHY/ANI checksums, magic/version fields and declared MDL length.
- `hl2m_engine_apply_vta_flex()` applies a VTA flex target to an engine model with fixed-point blending.
- `hl2m_flex_state_from_qc()`, `hl2m_flex_state_set()`, `hl2m_flex_state_clamp()`, `hl2m_flex_eval_weights()`, `hl2m_engine_apply_flex_weights()` and `hl2m_engine_apply_flex_controllers()` evaluate QC flexcontroller/flexrule state and apply VTA targets with fixed-point weights.
- `hl2m_collect_qc_sequence_events()` exposes QC sequence events as an engine-side event stream for frame windows.
- `hl2m_ik_pose_from_smd_frame()` and `hl2m_solve_ik_rules()` provide a deterministic fixed-point IK metadata/runtime bridge for QC IK chains/rules.
- `hl2m_build_hitboxes_from_qc_smd()` builds QC-authored or auto-generated hitbox tables before MDL writing.
- `hl2m_build_convex_decomp_from_smd()` builds deterministic fixed-point collision hull groups from SMD geometry.
- `hl2m_convex_decomp_split_long_axes()` refines hulls by recursively splitting long axes for more granular deterministic collision cooking.
- `hl2m_build_studiomdl_parity_report()` summarizes which StudioMDL-style features survived QC/SMD/VTA -> binary -> runtime bridge.
- `hl2m_write_phy_from_convex_decomp()` writes those hull groups into the compact PHY path used by the compiler.
- `hl2m_vta_write()` exports VTA text for round-trip tests.
- `hl2m_smd_frame_range()`, `hl2m_sample_smd_pose_lerp()` and `hl2m_sample_smd_pose_time()` sample SMD animation frames into engine pose data.
- `hl2m_pose_matrices_from_pose()` and `hl2m_pose_inverse_bind_from_pose()` build fixed-point pose matrices for runtime animation.
- `hl2m_engine_cpu_skin()` applies weighted CPU skinning to `hl2m_engine_model` buffers using fixed-point matrices.
- `hl2m_event_cursor_init()` and `hl2m_event_cursor_advance()` provide loop-aware sequence event playback.
- `hl2m_select_lod_from_distance()` chooses QC-derived LODs for runtime asset selection.
- `hl2m_blend_poses()` and `hl2m_apply_additive_pose()` provide fixed-point pose blend/additive helpers for layered animation graphs.
- `hl2m_build_attachment_poses_from_qc()` resolves QC attachments onto current pose matrices for muzzle flashes, props, tags and camera points.
- `hl2m_transform_hitboxes_to_world()` transforms decoded/authored hitboxes into world-space AABBs for engine collision queries.
- `hl2m_engine_filter_draws_for_lod()` compacts draw calls to the selected runtime LOD while keeping caller-owned static buffers.
- `hl2m_runtime_eval_sequence()` is a one-call runtime path for sample pose -> IK -> flex -> CPU skin -> events -> LOD filter.

## Rules kept

- C89-oriented code.
- No dynamic allocation calls.
- No C floating-point types or operations in the library.
- Source numeric fields encoded as IEEE-754 binary32 are converted directly to signed 16.16 fixed point with integer bit operations.
- All decoded output goes into caller-visible fixed-capacity structs from `include/hl2modelib.h`.

## Build

```sh
make
make check
```

## Probe tools

```sh
./hl2m_probe model.mdl
./hl2m_probe model.vvd
./hl2m_probe model.dx90.vtx
./hl2m_probe model.smd
./hl2m_probe material.vmt
./hl2m_probe model.qc
./hl2m_probe model.phy
./hl2m_probe model.ani
./hl2m_probe model.vta
./hl2m_compile_probe model.qc ref.smd anim.smd flex.vta
./hl2m_parity_probe model.qc ref.smd anim.smd flex.vta
./hl2m_runtime_probe model.qc ref.smd anim.smd flex.vta
./hl2m_animskin_probe model.qc ref.smd anim.smd flex.vta
./hl2m_sequence_runtime_probe model.qc ref.smd anim.smd flex.vta
./hl2m_bundle_probe model.mdl model.vvd model.dx90.vtx
./hl2m_smd_engine_probe model_ref.smd [models/path/name.mdl]
```

The probes use static file buffers. They do not allocate dynamically.

## Pairing MDL/VVD/VTX

Source model geometry is split. The `.mdl` works as metadata/index hub, `.vvd` stores vertex payload, and `.vtx` stores optimized draw topology / remaps. Use:

```c
hl2m_check_mdl_vvd_vtx_sync(&mdl, &vvd, &vtx);
```

to verify the matching checksum across the three decoded files, then:

```c
hl2m_build_engine_model(&mdl, &vvd, &vtx, 0, &engine_model);
```



## 1.0.0 additions

This pass turns the scattered runtime helpers into a more complete engine step:

- Added pose blending and additive pose application helpers for animation layers.
- Added QC attachment resolution against current fixed-point pose matrices.
- Added world-space hitbox transform helper for decoded/generated hitboxes.
- Added draw filtering for selected LODs, including fallback to LOD 0 when a source model only has base draws.
- Added `hl2m_runtime_eval_sequence()` as a compact engine-facing step that samples SMD animation, applies IK metadata, applies VTA/flex controller state, runs CPU skinning, collects sequence events and filters LOD draws.
- Expanded StudioMDL parity bits with pose blending, attachment runtime, hitbox runtime, draw filtering and sequence evaluation.
- Added `hl2m_sequence_runtime_probe`, and updated the synthetic parity QC with an attachment so runtime parity can now report no missing bits in the synthetic suite.

Honest scope note: this is still clean-room deterministic support for an engine pipeline, not byte-exact reproduction of Valve's full StudioMDL/runtime internals. The new layer is meant to be predictable, portable and static-buffer friendly.


## 0.9.0 additions

This pass pushes the library from “compiled bundle plus runtime metadata” into a more game-engine-ready animation path:

- Added SMD animation frame range detection and fixed-point pose sampling by frame pair or frame-time.
- Added fixed-point 3x4 pose matrices, including local/world pose construction and inverse bind construction.
- Added CPU skinning over `hl2m_engine_model` vertices using Source-style bone weights already decoded from SMD/VVD.
- Added loop-aware sequence event cursor helpers for runtime playback.
- Added QC compile-option LOD selection by fixed-point distance.
- Extended StudioMDL parity reporting with animation sampler, CPU skinning, event cursor and LOD runtime bits.
- Added `hl2m_animskin_probe`, which verifies pose sampling, CPU skinning, event playback, LOD selection and the expanded parity report under `make check`.

Honest scope note: this is still deterministic clean-room runtime support, not an exact Source runtime clone. The skinning path expects caller-owned bind/pose matrices and keeps all math fixed-capacity/fixed-point.

## 0.8.0 additions

This pass adds the first explicit runtime parity helpers:

- Fixed-point QC flexrule evaluator with a small stack VM for controller names, constants and add/sub/mul/div/min/max/clamp/neg operations.
- Separate flex weight evaluation and engine-side weight application helpers.
- QC event stream collection for frame windows.
- SMD frame to IK pose extraction plus deterministic IK rule solver/report helper.
- QC/SMD hitbox builder and long-axis convex hull splitter.
- StudioMDL parity report helper and `hl2m_runtime_probe`.

## 0.7.0 additions

This pass pushes the compile path toward stricter StudioMDL-style parity while keeping the library clean-room, fixed-capacity and engine-facing:

- Added structured QC capture for `flexcontroller`, `flexrule`, sequence `event`, `ikchain` and `ikrule` commands.
- Added MDL writer/decode coverage for multiple QC hitboxes instead of only one generated hitbox.
- Added sequence event emission into generated MDL seqdesc event tables and decode back into `hl2m_mdl.events[]`.
- Added IK rule emission into generated MDL animdesc rule tables and decode back into `hl2m_mdl.ikrules[]`.
- Added fixed-point flex controller state helpers and an engine bridge that maps simple QC flex rules onto VTA flex targets.
- Added deterministic convex collision grouping modes: whole model, per bone, per material, and bone+material hull groups. The PHY writer can now emit the grouped hulls as multiple compact solids.
- Added `hl2m_parity_probe`, a regression probe that fails unless QC+SMD compilation preserves multiple hitboxes, a sequence event, IK metadata, flex controller/rule metadata and grouped PHY collision.

Honest scope note: this is still not byte-for-byte StudioMDL. The new convex path is deterministic hull grouping, not a full VPhysics/HACD-style convex decomposition solver, and IK/flex support is metadata/runtime-bridge oriented rather than full Source runtime behavior.


## 0.6.0 additions

This pass makes the compiler path safer and more runtime-facing:

- Added `hl2m_make_compile_options_from_qc()` to derive compile options from QC model/body/material/LOD/collision commands.
- Added `hl2m_validate_compile_inputs()` for preflight warnings: incomplete QC, mismatched animation skeletons, invalid bone references, weight repair needs and VTA source-vertex range errors.
- Added `hl2m_validate_binary_bundle()` for post-write checksum/version/length validation across MDL/VVD/VTX/PHY/ANI.
- Added `hl2m_vta_write()` for VTA text round-trip export.
- Added `hl2m_engine_apply_vta_flex()` so a game engine can test flex targets directly on `hl2m_engine_model` buffers using fixed-point blend weights.
- Added `HL2M_COLLISION_PER_BONE_AABB` for conservative per-bone PHY collision cooking. This is still not a full VPhysics convex decomposition, but it is more useful for ragdoll-ish/jointed proxies than one whole-model AABB.
- `hl2m_compile_probe` now exercises QC-derived LODs, bundle validation, VTA round-trip and flex application.


## 0.5.0 additions

This pass adds the first end-to-end binary compiler path:

- `hl2m_decode_vta()` for Source-style VTA/flex target text.
- `hl2m_write_mdl_from_qc_smd()` for IDST v48-compatible metadata/bones/material/bodypart/mesh/anim descriptor emission.
- `hl2m_write_vvd_from_smd()` for vertex payload emission.
- `hl2m_write_vtx_from_smd()` for bodypart/model/LOD/mesh/stripgroup/index topology plus material replacements.
- `hl2m_write_phy_from_smd()` for conservative AABB collision cooking into a compact VPHY-style wrapper.
- `hl2m_write_ani_from_smd()` for scan-compatible ANI blocks plus fixed-point baked pose payload.
- `hl2m_compile_qc_smd_to_binary()` as the high-level QC+SMD+VTA -> MDL/VVD/VTX/PHY/ANI bundle builder.
- New `hl2m_compile_probe` regression tool and synthetic `test_anim.smd` / `test.vta` inputs.

This is still not a byte-exact StudioMDL replacement; it is a clean-room, fixed-capacity binary writer suitable for simple engine-facing bundles and for pushing the compile pipeline forward.

## 0.4.0 additions

This pass pushes the heavy Source-side metadata needed by a real engine importer:

- `.phy` decoder for the Source physics wrapper header, checksum, solid count and compact `VPHY` / legacy solid header summaries.
- `.ani` companion decoder for IDST-style animation block files and external block range inspection.
- MDL local animation descriptor capture: fps, flags, frame count, animation block/index, section metadata, zero-frame metadata and simple local channel scans when animation data is embedded in the MDL.
- MDL local sequence descriptor capture: labels, activity names, flags, bbox, blend count, groups and blend parameters.
- VTX material replacement list decoder from `optimize.h` packed `MaterialReplacementListHeader_t` / `MaterialReplacementHeader_t`.
- Engine draw material-name resolver that applies VTX per-LOD material replacements.
- QC parser side tables for `$collisionmodel`, `$collisionjoints`, `$renamematerial` and `replacematerial`.
- Synthetic `.phy` and `.ani` regression inputs plus expanded VTX replacement regression.

## 0.3.0 additions

This pass pushes the library closer to “drop into a game engine”:

- SMD -> engine model builder.
- SMD writer / round-trip text exporter.
- Basic QC generator from SMD metadata.
- QC parser expanded for LOD replacement, attachments, hitboxes, definebones and poseparameters.
- VMT material descriptor extraction.
- Engine bounds and validation helpers.
- Debug OBJ text writer for quick inspection in external tools.
- New `hl2m_smd_engine_probe` example.
- More synthetic regression files under `examples/synthetic/`.

## Current scope

This is not a full StudioMDL clone yet. It decodes Source model pieces, builds engine-ready geometry from compiled triplets and SMDs, and writes/decompiles QC/SMD manifests. The library now has a binary writer path for QC+SMD(+ANI/VTA) into MDL/VVD/VTX/PHY/ANI, plus metadata-preserving flex/event/IK/hitbox paths and deterministic grouped collision solids. Production-grade byte-exact StudioMDL parity remains a larger future layer.

See:

- `docs/SOURCE_SDK_2013_FINDINGS.md`
- `docs/QC_AND_ENGINE_PIPELINE.md`
- `docs/V0_3_ENGINE_BRIDGE.md`
- `docs/V0_4_PHY_ANIM_VTX.md`
- `docs/V0_5_BINARY_COMPILER.md`
- `docs/V0_6_VALIDATED_COMPILER.md`
- `docs/V0_7_STUDIOMDL_PARITY.md`
- `docs/V0_8_RUNTIME_PARITY.md`
- `docs/V0_9_ANIM_SKIN_RUNTIME.md`
- `docs/V1_0_ENGINE_RUNTIME.md`


## T-103 real-corpus compatibility (1.0.2)

- Correct MDL/VVD/VTX reading, real Source 48/49 topology and UVs.
- Optional recovery of one-byte-truncated headers in certain archive entries, including VTF and VMT.
- Strict C89 BC1/BC3 VTF-to-RGBA decoder: `src/hl2m_vtf89.c`.
- Fixed-point decimal formatter overflow fix; portable OBJ + MTL model exporter.
- CLI preview support: `./hl2m_export_obj89 ...` and `./hl2m_vtf_to_tga89 ...`.
- All synthetic regressions + native BCn test via `make check`.

See [docs/T103_CORPUS_FIXES.md](docs/T103_CORPUS_FIXES.md) for examples, tested models, and limits.

### Source SDK Walk_F correction v1.0.7

Se corrigió el cálculo trigonométrico de `RadianEuler` y se añadió una regresión de rotaciones de 180°; el nuevo diagnóstico real del Tyrant, el frame rate nativo (40 FPS), la secuencia 9-way y los límites pendientes están en `docs/SOURCE_SDK_WALK_DIAGNOSTIC_v1_0_7.md`. Use el `master codec` anterior sin romper API.

## v1.0.9: Source VMT/VTF/VVD shading pipeline

New renderer-neutral C89 material bridge is `vendors/source/texturizer/hl2m_material_sampler89.c`.
The master exposes `hl2m_master_fragment89()` for tangent basis and albedo/bump/detail RGBA samples, then `hl2m_master_shade89()` for software illumination. An actual shadow visibility value comes **from the presenter**, not from the VMT.

`tyrant01` was validated with four real DXT5 RGB normal maps and five VMT materials. The original `tyrant` phase 1–3 `$bumpmap` lines are commented/not present and must be left disabled. See `docs/SOURCE_MATERIAL_PIPELINE_V109.md` for full matrix, caveats, API and test conditions.

Recompile engines linked against the v1.0.8 headers: `HL2M_MASTER_ABI89 == 0x0109` changes struct layout.

## v1.0.10: Smooth Source shading

Se anade el vendor `shaderer/hl2m_smooth89.c` al master; API de interpolacion normal/tangente/UV por pixel sin tocar la geometria ni inventar normales. Vease `docs/SOURCE_SMOOTH_SHADING_V110.md`. Preview externo comparativo en el paquete de pruebas.


## v1.0.21 — Source normalized pose parameter controllers

New vendored `vendors/source/animator/hl2m_poseparams121.c` reads the
`studiohdr_t` local pose parameter descriptors (offsets 300, 304; 20 bytes
per descriptor) with validated offsets, and exposes normalization,
denormalization and per-sequence axis mapping. The optional master API
`hl2m_master_sequence_controls121()` takes normalized Q16.16 settings
instead of physical degree/range values. It converts EACH sequence's own
`paramindex[0..1]`, including child autolayers, before evaluating its grid.
The external function supports a switch to include existing recursive layers.

This is an **MDL-local implementation**. Source `CStudioHdr` virtual model
remapping, `STUDIO_AL_LOCAL` layers, world-space sequences and advanced IK
remain outside this implementation. Existing master APIs are unchanged.
`HL2M_MASTER_ABI89` remains `0x0119` because existing public layouts were
not modified; this release only adds new functions/types.

Validate with `make check` under strict C89 and the optional real model probe:

```
make hl2m_pose_controller_probe121
./hl2m_pose_controller_probe121 nemesis_ft1_v2.mdl 33
./hl2m_pose_controller_probe121 hunterb.mdl 30
```

## v1.0.22 — Source virtual-model include animations

New opt-in `vendors/source/animator/hl2m_virtualmodel122.c`: parses
`$includemodel`, builds master/local bone, sequence, animation and pose-control
maps for one included MDL, and remaps inherited poses onto the main skeleton.
The master entry point is `hl2m_master_virtual_sequence122()`.

Test: `make check`; real corpus tools: `make hl2m_virtual_probe122
hl2m_virtual_obj122`. See `docs/SOURCE_VIRTUALMODELS122.md` for tested
pairs, API and limitations. There is no automatic file loading or full
Source IK/virtual-model multi-group parity. Existing C89 engine calls remain.

## v1.0.23 — virtualmodel multi-bank resolver

A new **opt-in**, strictly ISO C89 vendor `hl2m_virtualmodel123.c`
accepts **one root MDL and up to three directly declared `$includemodel` banks**.
It creates a shared sequence/animation catalog, local-to-global maps, pose
parameter range unions and name-based bone remaps. The master selects the
correct MDL/ANI for any virtual sequence and returns a pose in the root
skeleton's bone order. Clients own and keep all MDL/ANI buffers alive.

**Validation:** `make check` (including `test_virtualmodel123`), and the
portable `hl2m_virtual_probe123` tool. A reproducible controlled second-bank
animation test is `hl2m_virtual_real_twobank123` (requires RE input MDLs,
not distributed). See `docs/SOURCE_VIRTUALMODELS123.md`.

**Limits:** catalog must have all direct include resources explicitly bound;
no recursive includes, cross-group autolayers, Source SRC bone transforms,
Source virtual IK/physics, or guaranteed SDK exact animation output. Existing
v1.0.22 single-bank calls remain available; ABI constant is unchanged.
