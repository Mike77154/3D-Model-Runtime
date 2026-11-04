# v1.7 (additive)

- `ms3d_quat_slerp`: shortest-path spherical quaternion interpolation using CORDIC fixed point, with near-parallel NLERP fallback.
- `MS3D_ROTATION_QUAT_SLERP`: new animation mode; existing Euler and NLERP retained.
- `ms3d_skin_evaluate_blend`: blended *local joint poses* and translations before skinning.
- `ms3d_player_blend`, `ms3d_player_blend_clear`, `ms3d_player_crossfade_to_range`: manual and timed crossfades between host-defined timeline ranges.
- `gif_ms3d_render` material-state adapter and sorted translucent draw queue (no renderer or decoder dependency).
- C89 GCC/Clang unit tests; 157/157 full-corpus independent streaming checks; 6-frame previews with original UV and materials.
- All prior functionality retained. No new interpolation is claimed as pixel- or motion-identical to MilkShape.

---

## v1.6 — MS3D 1.8.5 spec audit, quaternion NLERP, weight fix

- Added integer quaternion C89 `gif_ms3d_quat` module and animation-mode selector. NLERP is an approximation; **no claim of true SLERP**.
- Corrected vertex-extra subversion-1 weights from mistaken 255-based residual to documented 100-based residual; synthetic regression added.
- Added format audit CLI for optional subversion inspection, trailing bytes, keyframe monotonicity and group-index hints.
- Kept all v1.5 UV, materials, MTL texture-copy, smooth normals and skinned-corner callbacks.
- Validation: 157/157 corpus model parses and 3-frame player traces, one external dwarf model; strict `-Werror -pedantic` tests.
- Textured `zmb_hs` frames rendered via OBJ/MTL as an independent integration smoke test.

# v1.5 — Animation Player, UV and Materials Stream

- New `gif_ms3d_player` C89/Q16.16 runtime for seek, elapsed-ms advance, loop and once, clipping playback ranges, and max duration from FPS/last keys.
- Frame callback gives materials (including image/alpha paths), actual group/material membership, skinned positions and normals, native UV per triangle corner; no OBJ intermediary for engines.
- Added frame probe CSV tool and 6-frame OBJ sequence export through the **new** v1.5 callback API.
- OBJ fallback material defined in MTL and `s off` for unsmoothed faces.
- Automated full corpus animation+UV+material regression 157/157 and texture QA of 2-material zombie.
- Existing v1.4 modules retained.

## v1.2 — Smooth shading / extended sample corpus (2026-10-10)

- Added graphics-API-agnostic C89 fixed-point corner normal generator:
  source, per-group smoothing, smooth-all, flat.
- Added shaded corner emission callback for vendorized engine integration.
- Updated OBJ export to write new normals and selectable smoothing modes.
- Added synthetic shared-vertex vs hard-edge normals regression test.
- Raised rotation/position keyframe capacities 6144 -> 8192 for `dwarf.ms3d`.
- Tested all 157 Night of the Zombies models plus 3 external references:
  160/160 parse, smooth normals, export.
- Added render_compare.py (optional visualization of same geometry with
  different lighting interpolation; no runtime dependency).
- No third-party loader implementation copied; all provided sources analyzed
  as references only.

# v1.1 — Night Of The Zombies corpus hardening (2026-10-10)

- 157/157 MS3D reales importados; 157/157 pasan probe de skeleton, skinning, keyframe sampling.
- Corrige sección comments v1 con 4 contadores cero (sin retroceder el cursor).
- Corrige mapeo de weights[3] a bone base y tres bones adicionales.
- Valida sólo influencias de bones extras con peso activo.
- Incrementa límites estáticos de vertices/triangles/group indices/keys.
- Añade exportador OBJ/MTL C89 preservando UV/normales por esquina.
- Añade ms3d_probe y regresión para comentarios vacíos.
- Añade benchmark externo del corpus y script de build MinGW32.

# Changelog

## v2 upgrade

Mejoras sobre el primer paquete:

- Agregado `ms3d_parse_ex()` con `ms3d_parse_info` para saber bytes usados, trailing bytes y secciones opcionales detectadas.
- Comentarios MS3D ahora se almacenan sin heap:
  - `group.comment`
  - `material.comment`
  - `joint.comment`
  - `model.model_comment`
- Vertex extras subversion 1/2/3 ahora tienen helper de influencias:
  - `ms3d_vertex_get_influences()`
  - normalización Q16.16
  - merge de bones duplicados
- Resolución de jerarquía:
  - `joint.parent_index`
  - validación de parent inexistente.
- Helpers fixed-point:
  - saturating add/sub
  - mul/div Q16.16 sin `float` ni `double`
  - lerp Q16.16
- Helpers de engine:
  - `ms3d_compute_bounds()`
  - `ms3d_sample_joint_local()` para keyframes locales.
- Test extra agregado:
  - comments
  - vertex extras
  - joint parent
  - joint extra colors
  - model extra
  - sampling de animación.

## v1.3 — skin, matriz jerárquica y seam-aware normals

- `gif_ms3d_skin.c/h`: construcción bind/global/inversa, orden topológico,
  interpolación temporal y transformaciones de posiciones y normales Q16.16.
- CORDIC de 16 iteraciones sin float/double ni math.h para Euler XYZ (RzRyRx).
- Normalizar influencias válidas; vértices sin huesos no se mueven.
- Caso de peso extendido cero => asignación rígida al bone principal.
- `MS3D_NORMAL_SMOOTH_WELDED`, hash estático de posiciones duplicadas.
- `ms3d_anim_obj` muestra la pose animada de un modelo y conserva sus faces.
- Nuevas regresiones de matriz, jerarquía invertida, culling de ciclos,
  rotación, traslación y suavizado de costuras sin cruzar grupos duros.
- 160/160 modelos pasaron los checks 0 y 500 ms (sin pérdida de triángulos).

## v1.4 — Texture UV/material binding (2026-10-10)

- Added `gif_ms3d_uv` (triangle-corner UV conversion, authoritative group material table, direct static/skinned callbacks) and optional `gif_ms3d_mtl` module (portable MTL writer and collision-safe original texture copies).
- Fixed animated OBJ export: it previously had VT/VN but no material bindings or textures; now animated frames preserve `usemtl`, `mtllib`, diffuse maps and alpha maps when available.
- Default C89 callback keeps MilkShape top-left S/T; OBJ adapter emits (S,1-T) exactly once.
- Texture aliases include output model name and material index, avoiding collisions across model snapshots.
- New tests `test_ms3d_uv` and full-corpus regression 157 models x 2 paths = 314/314 passing triangle/UV/normal counts.
- Recovered 131 distinct archived texture files referenced by models (533/534 texture slots resolve; missing pinet2_mask.tga), with original names and directory paths.
- Visual QA from C89 exports: zmb / zomb / zmb_hs with their authored images.
- Previous v1.3 features preserved; no repair or modification of original triangulation.
