# PSA deeper support

This version improves PSA handling without breaking the C89 / heapless / fixed-point contract.

## Added pieces

- Optional non-standard `SCALE*` chunk parsing into `psk3d_animset.scale_keys[]`.
- Pose scales in `psk3d_pose.scales[]`, defaulting to identity when a file has no scale tracks.
- Scale-aware local-to-global pose propagation.
- Scale-aware LBS point transformation for point skinning.
- `psk3d_extract_sequence_track_range()` to expose the raw contiguous track range for a sequence/bone.
- `psk3d_sample_sequence_mapped_scaled_ex()` as an explicit mapped sample path that preserves scale tracks.

## Track addressing

For classic PSA keys, a key index is addressed as:

```text
key_index = (first_raw_frame + frame_index) * animset.bone_count + bone_index
```

`psk3d_extract_sequence_track_range()` returns the first key for a given bone and the number of frames in the track.

## Scale keys

When `scale_key_count == 0`, all sampled pose scales are identity:

```text
(1.0, 1.0, 1.0) -> (65536, 65536, 65536)
```

When `SCALE*` chunks are present, scale tracks are sampled with the same frame/bone addressing as `ANIMKEYS` and linearly interpolated in Q16.16.

## Limits

- Rotation is NLERP or SLERP; translation and scale are linear fixed-point.
- Non-standard curve keys are still registered as unknown chunks unless they use the `SCALE*` prefix.
- Scale-aware normal skinning uses the rotation path; exact inverse-transpose normal scaling is not included yet.
