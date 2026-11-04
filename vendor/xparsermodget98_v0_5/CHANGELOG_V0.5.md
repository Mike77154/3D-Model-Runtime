# v0.5 - multi-clip / frame-pose recovery

v0.5 is based on v0.4 and leaves the proven text/binary/MSZIP parser behavior
unchanged. The new `xp_anim_pose` layer turns the parser's animation callbacks
into a runtime-friendly, caller-owned clip/key database and pose sampler.

## Added

- `include/xp_anim_pose.h`
- `src/xp_anim_pose.c`
- count-only animation inventory pass
- caller-capacity-driven storage for any number of Frames, AnimationSets,
  tracks and keys supported by the caller's arena/storage
- `xp_anim_clip_record.frame_count`
- source key count and tick range per clip
- stable track target -> Frame ID resolution
- random access with `xp_anim_sample_pose_frame()`
- whole-database streaming with `xp_anim_emit_all_poses()`
- complete local pose for every Frame node in every sampled frame
- Q16.16 interpolation for position and scale
- shortest-path normalized quaternion lerp for rotation keys
- exact/held matrix-key sampling for sparse matrix timelines
- explicit flags telling the runtime whether a pose came from bind fallback,
  S/R/T keys, matrix keys, interpolation, or held matrix state

## Timing contract

DirectX `.x` `TimedFloatKeys.time` is a positive DWORD tick. There is no generic
"display FPS" field. v0.5 defines `frame_index` as one integer tick from
`first_tick` through `last_tick`, inclusive. `AnimTicksPerSecond` is preserved
when authored; if absent, no rate is invented.

## Regression

- v0.4 tests still pass.
- new multi-clip fixture: 2 clips, 2 tracks, 4 source keys, 6 reconstructed
  frames, 12 node poses.
- position keys at ticks 0/2 correctly reconstruct tick 1.
- sparse matrix keys at ticks 10/12 preserve tick 10 at tick 11 and flag that
  pose as `XP_ANIM_POSE_MATRIX_HELD`.
- random-access sampling and full streaming produce the same poses.

## Real integrated-animation checks

### dogs/dog.x

- nodes: 27
- clips: 1 (`AnimationSet0`)
- tracks: 26
- source keys: 698
- tick range: 0..53
- recovered frame count: 54
- emitted node poses: 1,458
- frame 0 reconstructed local matrices match bind locals exactly

### Zombie Apocalypse/mobzombie1.X

- nodes: 66
- clips: 1 (`Anim-1`)
- tracks: 34
- source keys: 116,994
- tick range: 0..3440
- recovered frame count: 3,441
- emitted node poses: 227,106
- frame 0 reconstructed local matrices match bind locals exactly

## Corpus/build policy

- external corpus remains 196/199; only the same three damaged DEFLATE files
  fail (`hostage.x`, `scout1.X`, `slowzombie3.X`).
- GCC and Clang: all 8 TEXT/BINARY/MSZIP feature combinations compile under
  `-std=c89 -Wall -Wextra -Werror -pedantic` (16/16).
- no malloc/calloc/realloc/free in `src/` or public headers.
- no C float/double or long long declarations.
- ASan+UBSan pass the v0.5 pose test plus the real dog and mobzombie1 assets.
