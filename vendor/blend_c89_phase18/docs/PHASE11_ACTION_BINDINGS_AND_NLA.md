# Phase 11 — Action Binding / Slot Selection / Narrow NLA Playback

**Scope:** extension of the Phase 10 codebase, binary SDNA parsing with no Blender executable. Source is **C89** (the precompiled third-party compression libraries remain separately licensed, with no guarantee of C89/heapless internals).

## Implemented

- `bc_anim_bindings.c`: follows serialized **Object.adt → AnimData.action → bAction** and maps the *actual* pointer to the Action ID used by the existing `--sample` command. Handles both pointer widths and byte orders with two 32-bit limbs. Does not assume Action ID zero.
- `--bindings FILE.blend OUT.tsv`: emits one record per Object with its name, active Action ID (or `-1`), slot handle when present, and NLA track/strip counts. The Action IDs are enumeration IDs of this reader, not Blender datablock names.
- `--nla FILE.blend OUT.tsv`: traverses the serialized NLA track/strip graph; writes object, track index, strip index, name, referenced Action ID, slot, start and end. Reports references, **not** a Blender NLA evaluation. Bounded traversal rejects cycles and missing Action references.
- `--play-obj FILE.blend FRAME OUT.obj auto`: resolves the **single armature object's active Action** via `AnimData.action`, and for Blender 4.4+ layered Actions, filters `ActionChannelbag` curves by assigned `slot_handle`. Supports previous explicit numeric Action IDs unchanged.
- `--play-sequence ... PREFIX auto`: resolves the same mapping independently at every requested frame, so a strip's time offset is applied to each frame correctly.
- **Minimal NLA clip path** when there is no simultaneous active Action: exactly one unmuted NLA track, exactly one clip strip with no strip FCurves/modifiers/metastrips, clip type 0, replace blend mode 0, no strip flags, influence 1, scale 1, repeat 1, and identical-length strip/action ranges. At frames within that clip, computes `action_frame = actstart + scene_frame - start`. All other arrangements return unsupported. This is *not* NLA mixing.

## Explicit limits

- This **does not reproduce a full Blender dependency graph** or realistic complex rig: NLA layers/blending/repeats/time warp, active Action + NLA mixing, IK, constraints, parent/child animation semantics beyond Phase 10, shape keys and driver evaluation remain unsupported.
- A Blender **object** with an Action does not imply that all FCurves are bone transforms. The skeletal player still only understands limited supported pose paths and will refuse incompatible paths.
- The present auto selector deliberately requires exactly one eligible armature with an Action/clip. Multi-rig scenes remain unsupported in auto mode; pass an explicit Action ID only when the old single-rig playback contract is met.
- In NLA mode, the clip must have zero flags, so even some innocuous Blender selection flags can make the preview refuse a valid Blender clip. This conservative refusal prevents misleading output.
- `--nla` is an exporter of serialized tracks, not a clip importer or NLA player for arbitrary files.
- Some legacy `.blend` formats lack Object.adt/AnimData entirely: Action binding inspection appropriately returns unsupported for those versions.
- The Win32 `build_win32_mingw32.bat` remains unverified here and accepts only uncompressed `.blend` files. Bundled Linux compression libs are not all-C89, and zlib/Zstd sources themselves are not vendored in a heapless form.

## Testing (executed in this environment)

Command: `make clean all` with `cc -std=c89 -pedantic -Wall -Wextra -Werror`.

- `tests/test_phase11_bindings.py`: 4 serialized pointer/endian variants; correct Action index and slot; NLA graph export; cycle and broken Action rejection; three real Blender startup scenes.
- `tests/test_phase11_auto_play.py`: rig with two Actions, second linked to armature, correct OBJ deformation; 4 pointer/endian combinations; single-strip NLA playback and 2-frame sequence; refuses active Action + NLA mixing, invalid Action and nonunit-scale strip.
- `tests/test_phase11_mutations.py`: 192 mutated binary command invocations completed without signal termination. Not a security audit.
- Regressions: Phase 09 Euler and Phase 10 object-space tests; Phase 08 compression tests; seven real static OBJ exports from supplied Blender source archives.
- Genuine Action bindings identified in source-archive `object_fracture/data.blend` from Blender 2.58/2.79, and camera actions in Blender 2.90.1 `thumbnailer.blend`.

**Not yet validated:** reproduction of an authentic Blender-made animated *armature* with NLA against Blender itself. The NLA motion outcomes above come from synthetic SDNA fixtures.

## Commands

```sh
make clean all
./blendc89 --bindings scene.blend bindings.tsv
./blendc89 --nla scene.blend tracks.tsv
./blendc89 --play-obj rig.blend 5 frame5.obj auto
./blendc89 --play-sequence rig.blend 1 24 clip auto
python3 tests/test_phase11_bindings.py
python3 tests/test_phase11_auto_play.py
python3 tests/test_phase11_mutations.py
```

## Next

1. Validate with user-produced, genuine `.blend` containing 1 armature + 1 mesh + 1 simple Action + 1 strip, using Blender as numerical reference.
2. Expand to multiple rigs and per-object deformer binding (the current single-rig restriction is intentional).
3. Add safe clip timing `repeat`, `scale`, `actstart/actend` remapping and conservative NLA blend support.
4. Continue replacing external compression vendor dependencies with a verifiably C89/no-heap backend if those constraints must apply to third-party code too.
