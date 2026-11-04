## v1.0.24 — Recursive included models and cross-group auto-layers

- Source SDK `studio_virtualmodel.cpp`-style indirect include traversal (root + up to three banks), depth-first catalog union, diamond reuse, cycle/missing/orphan/duplicate safeguards. Old multi123 stays unchanged.
- Source `bone_setup.cpp` `iRelativeSeq` resolution across overridden virtual groups and opt-in cross-group auto-layer animator with correct target-group pose controls and projected bone masks.
- New C89 tests for a four-model diamond, sequence overrides and transactionality. Real Resident Evil zombie/Hunter probes; controlled in-memory Hunter forward override/cross-bank layer with original animation bytes. Original user files never modified.
- Remaining limitations: group cap 4; WORLD/LOCAL, IK, full SRC-bone transforms and arbitrary CStudioHdr hierarchy not implemented. Some genuine Hunter DELTA layers return UNSUPPORTED until fully matched to Source.
- Clean archive build with strict C89 -Werror -pedantic.

## v1.0.23 — Multi-group virtual Source animation catalogs

- New independent `hl2m_virtualmodel123.c`, with up to four MDL groups (base + 3 direct includes). Shared sequence, animation and pose-parameter catalogs; Source-style deduplication and `STUDIO_OVERRIDE` support.
- Per-group named bone maps, parent conflict detection, and normalized pose controls translated across merged physical parameter ranges.
- New opt-in `hl2m_master_virtual_sequence_multi123()` dispatches to the owning bank's existing Source sequence evaluator and remaps its pose to the base skeleton; accepts external ANI supplied by the client.
- Reentrant, transactional multi-catalog builder and explicit validation of all declared bank paths. Retains fixed buffers, 32-bit fixed-point math, all prior vendors and APIs.
- New three-group deterministic regression and console diagnostics. Tested single real RE bank and controlled second bank with authentic MDL animation bytes. Real Nemesis models declare two banks that are absent from the provided corpus; their two-bank animation playback is **not** verified.
- Strict C89 (`-Werror -pedantic`) verified, no malloc/free, float/double or 64-bit math.

## v1.0.22 — Source virtual-model first stage

- Added strict C89 single-include virtual catalog: `$includemodel` path extraction, by-name bone remap, deduplicated sequences and animations, pose-control union.
- Opt-in virtual-sequence execution and BASE pose reconstruction; parent conflict safety checks.
- Verified RE zombie01 with mbczom_anims and REO Hunter with anim_hunter + external ANI; both exported real animated meshes without missing bones.
- Added negative tests for malformed MDL pointers, unknown include paths, duplicate sequence override, mismatching hierarchy, divergent normalized pose ranges.
- All earlier interfaces and vendors preserved.

## v1.0.20 — SDK pose-driven autolayers and bounded recursive accumulation

- Opt-in recursive sequence evaluator; physical `STUDIO_AL_POSE` control, cycle inheritance, per-bone weights; cycle protection and explicit unsupported statuses.
- Fixed Source `poseKey` stride/count mismatch and exact bounds validation.
- All v1.0.19 interfaces preserved, strict C89.

## v1.0.19 — Source SDK sequence bone weights and additive layer repair

- Fix native DELTA fallback quaternion bug. Unanimated bones now remain identity/zero, not bind quaternion, as described by Source's `CalcAnimation`.
- Add separate C89 vendor `hl2m_sequence_layers89.c` for per-bone weights, posekeys, physical pose parameters, layer envelopes, spline ramps, XFADE, NOBLEND, delta/POST masks, bounds checks.
- Master exposes metadata, physical pose sequence evaluation, masked overlay and opt-in one-level automatic autolayer composition.
- Verify 8 Birkin G1 real sequences using `add_eyeball`: changed local bone matrices drop from erroneous 80 to four, preserve 157171 triangles and bounding box; Hunter and Nemesis unaffected when no autolayers exist.
- Add isolated regression tests for empty DELTA animdesc and synthetic bone masks/pose parameters.
- ABI 0x0119. Explicit limits (not full Source virtual model, IK, recursive / local / pose-driven layers).

# v1.0.18 — Shortest-arc QuaternionSlerp in the Source sequence runtime

- New compile/link independent `hl2m_quaternion89.c` under `vendors/source/animator/`.
- Fixed Q16.16 matrix↔quaternion conversions, CORDIC `sin/cos`, numerical inverse cosine,
  quaternion slerp, endpoint validation, and lerped position; no float/double/int64/heap.
- `hl2m_sequence_blend_local89` and `hl2m_sequence_add_delta89` now share the new
  interpolation vendor; backward-compatible public sequence and master calls.
- Introduced `tests/test_quaternion118.c` and integrated it into `make check`.
- Real Hunter β 25%, 50%, 75% attack cycle exported through MDL/VVD/VTX
  skinning with 0 missing bones. This is a structural check, not proof of Valve parity.
- Full previous C89 regression test suite passed. No VTF/VMT/mesher modifications.
- ABI version constant 0x0118; patch 18. Client relink advised.
- Does NOT solve anomalous input translations in Licker or automated gesture ordering.

# v1.0.17 — Large Source 49 models and unsigned VVD bones

- Seven Resident Evil Workshop corpora, 30 complete bundles, 2,097,072 triangles, 472 raw motions tested.
- Expanded static VVD/VTX/engine capacities to handle G1/G2/G3, Nemesis and Super Tyrant.
- Fixed signed VVD bone indices above 127; G1 missing skinning influences 49,679 -> 0.
- Added a C89 regression; preserved previous vendors/master functions; ABI 0x0117.
- VTF exceptions: 2 identical cubemaps, not decoded as ordinary 2D textures.

## v1.0.16 — Sequence/gesture runtime from SDK audit

- New independently compiled C89 `hl2m_sequence_eval89.c`, master sequence and gesture entrypoints.
- Fractional-cycle interpolation of local transforms; animated Source section/ANI read paths preserved.
- 1D/2D pose grid sampling; orthogonalized approximate matrix blend, not exact Source quaternion slerp.
- Explicit, per-bone additive delta overlay, not automatic gesture layering.
- Real Resident Evil (T-103, Ustanak, Hunter beta, Lisa) and existing regression tests; compare gallery and example scripts.
- `HL2M_VERSION_PATCH=16`, `HL2M_MASTER_ABI89=0x0113`.

# v1.0.15 — Source SDK animation anomaly audit (Licker / Hunter)

- SDK review: `CalcBonePosition` uses compressed value times native `posscale` (and `RAWPOS` reads `Vector48`); extreme values were confirmed IN THE INPUT MDL. The SDK's `pLinearBones` also references the same suspect position scales on the Licker.
- Added explicit `hl2m_master_anim_guard89()` and `hl2m_native_sample_anim_matrices_guard89()` with a caller-chosen local translation delta threshold and `hl2m_anim_safety_report89`.
- The **default/native playback path is unchanged** except for a safe saturating integer addition instead of signed-overflow undefined behavior.
- Guard-mode fallback is deliberately **NOT Source-authentic**: only flagged position components return to their reference pose. No new IK, forced pose or per-asset hardcoding.
- `hl2m_export_animated_obj89` accepts `--guard-outliers [units]` to label recovered previews and report modified bones.
- `hl2m_anim_guard_probe115` reports raw versus guarded matrix ranges and the first affected bone.
- Synthetic guard regression and real Licker/Hunter/T-103 sweep; existing vendors preserved.
- No `float`, `double`, dynamic allocation or 64-bit arithmetic added to the C89 library.

# v1.0.14 — Resident Evil animation sections + ANI blocks + corpus stress tests

- Secciones embedded `mstudioanimdesc_t::pAnim` con índice de bloque, fotograma local y última muestra especial.
- Master API aditiva `hl2m_master_anim_external89` para bancos `.ani` externos (sin tocar los puntos de entrada anteriores).
- Test C89 sin corpus `tests/test_anim_sections114.c`; sondas de cuerpo/canales/texturas por lotes.
- `hl2m_vtf_to_tga89`: buffer de muestra de 32 MiB y límite de 4096x4096, no altera ABI de la biblioteca.
- Corpus RE (sin HOTD): 146 MDL, 2329 motions muestreadas, 848 VTF únicos decodificados.
- Pendiente validación visual del Licker, Hunter `reo_hunter`, mezcla Source e IK avanzado. NUNCA equiparar lectura con fidelidad visual.

# v1.0.13 — HOD2 cross-model QA

- Verified 25/25 Source v48 HOD2 model sets with both DX80 and DX90.
- Verified 321 VMT and decoded 321 DXT5 VTF, incl. 83 alpha-tested materials.
- Found only `@ragdoll`/`@idle` tracks; 25/25 models have identical
  mesh output at frames 0/50/100, not usable creature attack cycles.
- Fixed hardcoded `t103_native` OBJ object names in both exporters.
- Added `hl2m_corpus_metadata113` and portable corpus-batch QA script.
- Core codec format readers unchanged; previous vendor set and ABI preserved.

# v1.0.12 — Resident NPC + Chronicles corpus audit

- Tested 100 MDL files from two user-provided archives (83 Source 48/44 and 17 Source 49). All 100 metadata decodes passed. 91/91 complete MDL+VVD+VTX bundles produced geometries, zero mesher warnings or discarded triangles; 9 were animation-only/incomplete and were not falsely counted as mesh passes.
- Corrected **signed negative relative VTX offsets**, including material-replacement name pointers. Support validated 27-byte SDK and extended 35-byte VTX strip headers, detected via bounded header validation when more than one strip is present.
- Corrected `hl2m_write_vtx` replacement table structure: headers contiguous, names stored separately, as expected by Source.
- Corrected VVD fixup repacking for vertices **and tangents**, preserving per-LOD ordering; this removed stretched geometry from the Nemesis case. The previous loader read fixup metadata but did not apply it.
- Raised compile-time static caps: VVD vertices 16,384 -> 32,768; VTX accumulated indices 65,536 -> 262,144; engine draw vertices 65,536 -> 98,304. Still no heap allocations.
- Added uncompressed VTF BGRA8888 (12), ABGR8888 (1) and BGR888 (3) decode. Original DXT1 and DXT5 preserved. 554/554 VTF headers parsed and six sample textures fully decoded. 438/438 VMT text materials parsed.
- Added three independent synthetic C89 regressions (`test_corpus112`, `test_vvd_fixup112`, `test_vtf_uncompressed112`), plus a Python offline corpus harness.
- Preserves the existing public APIs and vendor modules. **Rebuild consumers** because capacity increases alter public struct layouts (master ABI 0x0112).
- Scope: geometry assembly and texture parsing validated broadly; animations were inventoried (1,195 raw animations, 977 sequences), NOT rendered/tested exhaustively. The gallery is an untextured offline geometry preview, not Source shader parity.

# v1.0.11

- Source SDK `GetGlobalVertexIndex`/`GetGlobalTangentIndex` model-relative
  offsets used for VVD vertex and tangent binding.
- New standalone C89 UV-filter sampler (nearest preserved, bilinear added).
- Optional master `hl2m_master_smooth_fragment_linear89` uses fragment UVs.
- Tests for texel centers, V orientation, repeat/clamp and bilinear values.
- Phase1 T-103 coat UV auditing: 2,757 triangles/1,844 unique VVD vertices;
  compare VMT/VTF atlas against material and UV island overlay.
- Does not claim to reproduce full Source filtering/mipmap or DX9 shader.

## v1.0.10 - Smooth shading / raster pipeline
- Interpolacion de normales VVD, tangentes y UV por fragmento, C89 estricto.
- Se preservan limites de materiales y costuras de tangentes.
- Prueba de la ruta de smooth con el master y visual sobre T103.
- Geometria, animacion, materiales y APIs previas intactos.

# v1.0.8 — Native facial RLE precision fix (2026-10-09)

- Studio MDL decoder retains raw IEEE-754 `posscale`/`rotscale` bit patterns.
- Animator multiplies each RLE sample by the original scale before Q16.16
  rounding; its unsigned 32-bit high/low product avoids C floating point,
  64-bit integers, and heap allocation.
- Restored T-103 jaw `ago`, lip `lip` and eyebrow `eyebrow` channels in
  `Rage`, `Attack_Stand_LH`, `Attack_Stand_RH`, `Attack_Stand_Back`, `Attack_Run`.
- RAWROT2 path retained. Existing modules and formats preserved.
- Includes fixed-point scale arithmetic regression tests, native facial
  probe and real-corpus before/after visual test.
- ABI changes due to additional `hl2m_mdl_bone` fields; rebuild users.

# v1.0.6 - IK + PHYSIQUER (2026-10-09)

- New fixed-point two-segment solver, anatomical knee direction, preserved
  endpoint orientation, local/world consistency and descendants.
- New ground-oriented character kinematic movement + foot contact anchor.
- Master 0x0106 additive APIs; previous exported functions retained.
- Offline OBJ `--ik-grounded` diagnostic exporter and real corpus numerical tests.
- Source SDK `bone_setup.cpp` compared; documented unsupported parts.

# v1.0.5 (additive)
- Added physics, IK and shaderer as separately compiled C89 vendor modules.
- Moved SMD/VTA/QC decoder/writer code into per-format `.inc` implementations.
- Added linkable SMD/VTA/QC/ANI vendor interfaces; master now invokes them.
- Separated VMT effect extraction from software fragment shading.
- Added master calls and test_vendors105 end-to-end regression.
- Preserved previous API symbols and MDL/VVD/VTX/PHY/VTF decoding.
- Scope: physics is AABB/ground; IK is root-contact/QC metadata, not Source VPhysics/IK parity.

## 1.0.4 — Source vendor modules and movement curves (2026-10-09)

- Added `include/hl2m_master89.h`, master codec dispatcher for legacy subformats,
  skeleton/skin, VTF RGBA and VMT material FX.
- Split MDL, VVD, VTX, VMT, PHY and ANI parser implementations into vendored
  `.inc` modules; separated mesher and CPU skinner implementations. They are
  still compiled in one private-support translation unit (not standalone `.o`).
- Moved independent VTF, native animator and texturizer into vendor dirs.
- Added actual MDL movement-curve sampling (Source SDK `Studio_AnimPosition`)
  with `hl2m_master_motion89()` and optional exporter `--entity-motion`.
- Corrected `STUDIO_ANIM_DELTA` missing channel behavior, regression tested.
- Preserved all existing public functions and old formats.
- Validated seven supplied T-103 Source 48/49 model sets; native movement
  samples for all three Walk_F clips; strict host GCC C89 regression suite.

## v1.0.3 — Native Source animations and shading material FX

- Added native RLE/Euler, packed quaternion 48/64 animation sampling, bind matrices and native MDL `poseToBone` inverse bind for accurate weighted skinning.
- Corrected global VVD bone attribution vs local VTX strip palettes and transformed skinned normals.
- Added portable C89 VMT material FX parsing, bump/normal mapped software fragment lighting, Phong, rim, environment/detail and alpha test.
- Added `hl2m_export_animated_obj89`, `hl2m_vmt_fx_probe89`, `hl2m_shader_demo89`, and shader/native T-103 tests.
- Verified T-103 phase 1/2/3 posed exports, eight walking frames of phase 1, and existing synthetic regression suite.
- Advanced GPU parity, all external animation-block formats and full animation blending remain future work.

# 1.0.2 — T-103 Source 2013 corpus and VTF support

- Fixed studiohdr byte offsets and native VVD magic, Source v49 read support.
- Fixed fixed-point decimal formatter integer overflow.
- Added recoverable one-byte-truncated MDL, VVD, VTX, VTF, VMT headers.
- Added standalone ANSI C89 VTF BC1/BC3 decoder and TGA example.
- Added OBJ/MTL export, native tests and T103 corpus documentation.

# Changelog

## 1.0.0

- Added `hl2m_blend_poses()` for fixed-point pose blending across matching bone names.
- Added `hl2m_apply_additive_pose()` for deterministic additive animation layers.
- Added QC attachment runtime resolution through `hl2m_build_attachment_poses_from_qc()`.
- Added `hl2m_transform_hitboxes_to_world()` for world-space hitbox AABBs from pose matrices.
- Added `hl2m_engine_filter_draws_for_lod()` for runtime LOD draw selection.
- Added `hl2m_runtime_eval_sequence()` as a one-call engine step: sample pose, IK, flex, CPU skinning, sequence events and LOD draw filtering.
- Expanded StudioMDL parity report flags for pose blending, attachment runtime, hitbox runtime, draw filtering and sequence evaluation.
- Added `hl2m_sequence_runtime_probe` and wired it into `make check`.

## 0.9.0

- Added SMD animation frame range detection and fixed-point pose sampling helpers.
- Added fixed-point 3x4 pose matrices, world-pose construction and inverse bind construction.
- Added `hl2m_engine_cpu_skin()` for weighted CPU skinning on engine model buffers.
- Added loop-aware sequence event cursor helpers for runtime playback.
- Added QC-derived runtime LOD selection by fixed-point distance.
- Expanded StudioMDL parity bits for animation sampling, CPU skinning, event cursor playback and runtime LOD selection.
- Added `hl2m_animskin_probe` and wired it into `make check`.

## 0.8.0

- Added fixed-point QC flexrule evaluator with a small stack VM for controller names, constants and RPN-style add/sub/mul/div/min/max/clamp/neg operations.
- Added explicit flex weight export and engine-side weight application helpers, so engines can cache controller evaluation separately from mesh deformation.
- Added QC event stream collector for frame windows.
- Added SMD frame -> IK pose extraction plus deterministic IK rule solver/report helper for QC IK chains/rules.
- Added QC/SMD hitbox builder that preserves authored `$hbox` data or auto-generates per-bone boxes when QC has none.
- Added long-axis convex hull splitter for more granular deterministic PHY cooking inputs.
- Added StudioMDL parity report helper that summarizes QC structured features, generated binary tables and runtime bridge coverage.
- Added `hl2m_runtime_probe` to make `make check` verify flex evaluation, event collection, IK solve, hitbox build, convex splitting and parity reporting.


## 0.7.0

- Added QC side tables for flex controllers, flex rules, sequence events, IK chains and IK rules.
- Added MDL writer/decode coverage for multiple QC-driven hitboxes.
- Added sequence event emission into generated MDL sequence descriptors.
- Added IK rule emission into generated MDL animation descriptors.
- Added fixed-point flex controller state helpers and engine-side flexcontroller/flexrule application over VTA targets.
- Added deterministic grouped collision hull building from SMD geometry: whole model, per bone, per material and bone+material groups.
- Added PHY writer path from grouped collision hulls, yielding multiple compact solids when the source geometry has multiple groups.
- Added `hl2m_parity_probe` plus `examples/synthetic/test_parity.qc` to guard StudioMDL-parity features in `make check`.


## 0.6.0

- Added QC-to-compile-options bridge for model name, body name, surface material, LOD switchpoints and collision hints.
- Added compile input validation for QC completeness, animation skeleton mismatch, invalid bone references, weight normalization issues and VTA vertex range checks.
- Added binary bundle validation for MDL/VVD/VTX/PHY/ANI magic/version/checksum/length coherence.
- Added VTA writer and VTA decode/write/decode round-trip coverage.
- Added engine-side VTA flex application with fixed-point blend weights.
- Added conservative per-bone AABB PHY cooking mode.
- Expanded `hl2m_compile_probe` to test QC-derived LODs, bundle validation and flex application.

## 0.5.0

- Added VTA/flex target text decoder.
- Added first end-to-end QC + SMD + optional animation SMD + optional VTA binary compiler path.
- Added MDL writer for bones, textures, cdmaterials, hitboxes, bodyparts/models/meshes and animation/sequence descriptors.
- Added VVD writer for fixed-point vertex payloads converted to Source-style binary32 bits.
- Added VTX writer for LODs, triangle-list stripgroups, indices/remaps and material replacements.
- Added PHY writer with conservative AABB collision cooking into a compact VPHY-style wrapper.
- Added ANI writer with scan-compatible channel headers and appended fixed-point baked pose payload.
- Added `hl2m_compile_probe` and synthetic VTA/animation SMD regression inputs.

## 0.4.0

- Added `.phy` decoder for wrapper header, checksum, solid count and VPHY/legacy solid summaries.
- Added `.ani` decoder for IDST-style animation block companion files.
- Added MDL local animation descriptors, local sequence descriptors, animblock table metadata and simple embedded animation channel scans.
- Added VTX per-LOD material replacement table decoding.
- Added engine draw material-name resolver with replacement material support.
- Added QC structured side tables for collision sources and material replacements.
- Expanded probes and synthetic regression inputs for `.phy`, `.ani`, and VTX material replacement lists.

## 0.3.0

- Added direct SMD-to-engine-model builder.
- Added SMD writer for text round-trip.
- Added basic QC generator from SMD metadata.
- Expanded QC command table.
- Added structured QC side tables for LODs, attachments, hitboxes, definebones and poseparameters.
- Added VMT material descriptor extraction.
- Added engine model bounds and validation helpers.
- Added debug OBJ writer for engine model buffers.
- Added `hl2m_smd_engine_probe` example.
- Added synthetic SMD/VMT/QC regression inputs.

## 0.2.0

- Added QC parser/writer/decompiler foundation.
- Added MDL+VVD+VTX engine mesh builder.
- Added bundle probe example.

## 0.1.0

- Initial MDL, VVD, VTX, SMD and VMT decoders.

## v1.0.7 — Source SDK walk diagnostic / corrected rotation pipeline

- Fixed the Q16.16 RadianEuler sine polynomial beyond +/-pi/2; previously generated severely non-orthonormal matrices for legitimate native rotation channels.
- Added `tests/test_source_rot107.c`, run from `make check`, testing pi/quadrant rotation invariants; remains strict ANSI C89.
- Investigated original Source SDK `CalcBoneQuaternion`, `CalcAnimation`, `CalcPoseSingle` and `Studio_AnimPosition` with T-103 corpus; see `docs/SOURCE_SDK_WALK_DIAGNOSTIC_v1_0_7.md`.
- Built true 40 FPS / 57-sample Walk_F animation corpus preview with root motion and WITHOUT extra IK.
- Preserved master codec ABI 0x0106 and all existing vendor files; no feature claims about unimplemented sequence blending or exact Valve runtime parity.

## v1.0.9 — VMT/VTF/VVD source material integration
- Added `vendors/source/texturizer/hl2m_material_sampler89.c`, with Source-style path canonicalization, UV sampling and correct `VVD tangent.w` handedness.
- Added `hl2m_master_fragment89()` which binds caller-owned base/normal/detail texture RGBA buffers and tangent basis, without renderer dependencies.
- Normalized software bump normal after conversion to tangent space. Kept RGB VTF normals distinct from optional DXT5nm swizzling. Added `$halflambert` handling and externally-supplied shadow visibility.
- Added real-corpus test utilities for `tyrant01` head VMT/VTF and full Source49 model per-vertex lit PLY; normal shading affects 11061 vertices in that test.
- Added strict C89 regression `test_material_pipeline109` and SDK/material report in docs.
- **No claim** of DX9 shader bit-parity or actual GPU shadow-map generation.
- Bumped master ABI to 0x0109 (callers must recompile); previous exported functions retained.
