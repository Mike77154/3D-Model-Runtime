# v0.13 engine integration and validation

## API 1: image preflight

```c
C4D89_ImgInfo info;
unsigned long needed;
int rc = c4d89_img_probe_info(file_bytes, length, &info, &needed);
if (rc == C4D89_IMG_OK && needed <= preallocated_rgba_capacity) {
    rc = c4d89_img_decode(file_bytes, length, rgba_buffer,
                          preallocated_rgba_capacity, &info);
}
```

This enables predictable memory budgets for BMP/TGA/PNG/JPEG without heap allocation. PNG and JPEG headers are only checked enough to obtain image dimensions and supported subset; the full image decoder MUST still run. Unknown/malformed formats fail. Image metadata is returned in native unsigned fields; required bytes are `width * height * 4` for top-left RGBA8.

## API 2: batch scalar animation sampling

```c
C4D89_FrameStats stats;
int rc = c4d89_sample_animation_frame_file(open_c4d_file,
        &animation_index, frame_time_q16,
        1u, /* shortest-arc HPB mode (optional) */
        my_track_value_callback, user, &stats);
```

No global bone transform or skin deformation is implied by these scalar samples. The callback sees the original track index and track metadata. Irregular/unsupported stride tracks are counted and excluded; corrupted supported tracks return an error. File remains owned by caller and need not be reopened for each track.

## Correctness fixes

- TGA RLE packet crossing declared dimensions is rejected (`C4D89_IMG_CORRUPT`).
- BMP negative height derived via unsigned two's-complement conversion to avoid platform-dependent signed overflow.
- All v0.12 modules and reference assets are preserved.

## QA

The `test_v13_core.sh` suite runs 14 real fixtures (PNG, BMP, TGA and JPEG), rejects undersized output buffers, exercises RLE overflow, and compares two synthetic native-compatible key streams to a known Q16.16 frame result. It exercises callback interruption and optional angular mode. Prior v0.12 image and source tests remain available.

## Pending

The scene material BaseLink, parenting, skin weights and inverse binds remain unresolved. Neither preflight nor scalar interpolation solves those missing native relationships.
