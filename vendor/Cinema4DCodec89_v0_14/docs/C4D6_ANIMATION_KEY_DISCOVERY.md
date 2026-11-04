# C4D6 native animation: verified scalar key record (experimental)

These are measurements from the user's C4D6 game scene files, NOT official Cinema 4D file-format specifications.

## Observed keys

For `L_walk.c4d`, scalar animation channels `Position . X`, `Position . Y`, `Rotation . B`, `Rotation . H`, `Rotation . P` each have 60 native key records at a common **109-byte stride**.

The first Position X keys start at *native byte offsets* 134011, 134120, 134229; the 60th is at 140442 (first + 59 * 109).

```text
Offset +00  0c 00 02 01 00 01 af 50 00 00 00 00   verified 12-byte context
Offset +12  16 [f32_be numerator] [f32_be denominator]    time t=n/d (s)
Offset +21  16 [f32_be ?]         [f32_be ?]             undecoded
Offset +30  16 [f32_be ?]         [f32_be ?]             undecoded
Offset +39  13 [f32_be value]                            scalar value
Offset +44  13 [f32_be ?]                                undecoded
Offset +49  13 [f32_be ?]                                undecoded
Offset +54  ... additional record bytes (C4D6 metadata)
Next key    +109 (for this tested sample; different tracks may vary)
```

`0x16` rational times in the first channel are 0/1, 1/30, 1/15, 1/10, 2/15 ... . The last time = 59/30 seconds. Position X begins `0, 2.71036, 5.36803, 12.0003` in native units. Rotation channels contain numbers near `6.283` radians: **raw values**, not unwrapped angles.

`c4d89_anim.c` uses these markers to index each channel's keys and produce integer Q16.16 time/value pairs, without `float/double`, SDK or intermediate file. Its time division uses unsigned integer bit iterations (no 64-bit multiply). Every emitted key contains its exact native file offset.

## Full observed sample results

| File | Track channels | Key records |
|---|---:|---:|
| L_walk.c4d | 42 | 2,520 |
| L_walk_b.c4d | 163 | 10,269 |
| at_jump_knock_a.c4d | 218 | 25,724 |
| dead.c4d | 148 | 31,968 |
| appearance.c4d | 317 | 124,898 |
| SquirmingBodyBag.c4d | 16 | 3,248 |
| **Six tested clips** | **904** | **198,627** |

`L_walk.c4d` decoded track 0 was exported to `previews/L_walk_track_00.csv` and track 1 to `previews/L_walk_track_01.csv` for independent numerical inspection. Image `previews/Native_C4D6_L_walk_Animation_Keys.png` plots directly decoded values.

## Cautions

- `Position . X` repeats across many objects. Do NOT merge channels by name alone.
- A particular track's owning joint is not yet verified by a native object ID, so these tracks cannot correctly pose a skeletal mesh yet.
- The two additional `0x16` fields and two additional `0x13` fields are likely interpolation-related, but **not decoded or interpreted**. The engine must not assume equivalent Bezier interpolation.
- Detection checks an observed prefix and finite numeric values. It is an experimental C4D6 parser, not a general Maxon serializer.
- JPEG/media observations are independent of animation and are documented in v0.3 format notes.
