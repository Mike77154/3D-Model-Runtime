# Torque white-room oracle notes

These notes summarize the loader/runtime process used as a behavioral oracle. They describe the process, not copied engine code.

## DTS load path

```text
TSShape read
├─ read version word
├─ split exporter version and read version
├─ reject future/too-old formats
├─ read split-pool layout
│  ├─ pool32 starts at memory buffer base
│  ├─ pool16 starts at start16 dwords
│  └─ pool8 starts at start8 dwords
├─ for v29, decrypt/XOR memory buffer before pool interpretation
├─ endian-convert pools
├─ decode sequence tail
├─ decode material-list tail
├─ assemble shape payload from pools
│  ├─ read count block
│  ├─ pass guard checkpoints
│  ├─ read bounds/radius/tube radius
│  ├─ read nodes, objects, deprecated decals, deprecated IFL records
│  ├─ read subshape ranges
│  ├─ read default transforms and animated transform tracks
│  ├─ read object states, triggers, details
│  ├─ preserve v27+ vertex-format and packed vertex block
│  ├─ read mesh records by mesh type
│  └─ read names table
└─ initialize runtime derived data
   ├─ node child/sibling links
   ├─ object sibling links
   ├─ first translucent object per subshape
   ├─ detail poly counts
   ├─ alpha-in/alpha-out bands
   └─ detail lookup table
```

## DTS sequence order

For modern DTS sequence records, the white-room order is:

```text
name index
flags
keyframe count
duration
priority
first ground frame
ground frame count
base rotation
base translation
base scale
base object state
base deprecated decal state
first trigger
trigger count
tool begin
rotation matters
translation matters
scale matters
deprecated decal matters
deprecated IFL matters
visibility matters
frame matters
material-frame matters
```

`dirty_flags` are derived by the library after reading matters bitsets. They are not serialized as a separate field.

## DSQ import path

```text
import DSQ
├─ read single 32-bit DSQ version word
├─ read DSQ node-name table
├─ map DSQ node names onto DTS node names
│  ├─ exact match
│  └─ loose space/underscore match
├─ ignore tracks for unmapped nodes
├─ append sequence names to DTS name table
├─ append sequences to DTS sequence table
├─ remap matters bitsets from DSQ node indexes to DTS node indexes
├─ append rotation, translation, and scale tracks
├─ append ground transforms
└─ append triggers
```

## Runtime render planning path

```text
runtime build
├─ locate smallest visible detail
├─ build pixel-size detail lookup table
├─ detect screen-error LOD mode
├─ find first translucent object per subshape
├─ compute/keep detail poly counts
└─ build alpha transition defaults

runtime select
├─ projected radius path: pixel size = projected radius * 2 * detail adjustment
├─ pixel-size path: lookup detail level and intra detail
└─ screen-error path: choose first detail under tolerance

runtime render plan
├─ produce current detail span
├─ optionally produce next detail crossfade span
├─ split non-translucent/translucent object ranges
└─ emit draw commands as object/mesh/primitive/material/index ranges
```

The output is renderer-neutral. A host engine owns transforms, skinning, material instances, scene state, and GPU submission.

## DTS v29 pool XOR

Torque v29 applies a bytewise XOR over the split-pool memory buffer using a 256-byte key derived from application/password seed material. This library exposes:

```c
int dts89_make_v29_key(const char *seed, dts89_u32 seed_len, dts89_u8 *out_key, dts89_u32 key_len);
int dts89_decode_shape_with_key(const void *bytes, dts89_u32 size, dts89_arena *arena, dts89_shape *out_shape, const dts89_u8 *key, dts89_u32 key_len);
```

No application seed is embedded in the library.

## Runtime animation-planning path added in v3.4

```text
thread position update
├─ clamp normalized Q16.16 position to unit range
├─ cyclic sequence path
│  ├─ treat 1.0 as just-before-loop
│  ├─ key0 = floor(pos * keyframe_count)
│  ├─ key1 wraps to zero on the last key
│  └─ key_pos = fractional part
├─ one-shot sequence path
│  ├─ allow 1.0 as the final key
│  ├─ key0 = floor(pos * (keyframe_count - 1))
│  └─ key1 clamps to the final key
├─ ground path
│  ├─ first sample is implicit identity
│  ├─ ground_index0 may be -1 for that implicit identity
│  └─ ground_index1 resolves into the ground arrays
├─ node track resolution
│  ├─ rotation track = base_rotation + ordinal * keyframes + key
│  ├─ translation track = base_translation + ordinal * keyframes + key
│  └─ scale track chooses uniform/aligned/arbitrary arrays from sequence flags
├─ object track resolution
│  ├─ visibility object states
│  ├─ mesh frame object states
│  └─ material-frame object states
└─ trigger scan
   ├─ locate boundary trigger for previous and current positions
   ├─ emit forward or reverse event order
   └─ apply reverse inversion flag when walking backward
```

This is a renderer-neutral and transform-neutral plan. It gives an engine the same indexes Torque would use to fetch transform, object-state, ground, and trigger data, while keeping interpolation and final matrix construction in the host layer.

## Validation path added in v3.4

```text
post decode validation
├─ count and pointer sanity
├─ node/object/name references
├─ subshape ranges
├─ detail/object-detail feasibility
├─ mesh primitive/index/material ranges
├─ skin/sorted/decal extra payload sanity
├─ sequence track ranges
├─ ground and trigger ranges
└─ warning report for unusual but render-skippable details
```

The validator is intentionally tolerant where Torque can skip an object/detail combination at draw time. Those cases become warnings instead of hard errors.

## Deep runtime-oracle push added in v3.5

```text
Torque detail lookup parity
├─ smallest visible detail = minimum non-negative detail size
├─ lookup chooses the first detail where pixel_size > detail.size
├─ intra detail uses previous-detail boundary, or 2 * current size for the first detail
├─ screen-error mode uses averageError with Torque's legacy scale factor
└─ billboard details are treated as renderable detail entries, with optional translucent skipping

Torque alpha-crossfade parity
├─ if intra is past alpha-in + alpha-out, emit current detail opaque
├─ if intra is inside alpha-in, emit next detail fading in then current opaque
└─ if intra is inside alpha-out, emit next detail opaque then current fading out

Renderer-neutral primitive expansion
├─ draw commands still preserve object/mesh/primitive/material/index ranges
├─ optional helper expands triangle lists, strips, and fans
└─ output triangles are caller-buffered; no internal ownership is introduced
```

These pieces do not submit GPU commands. They reproduce the planning decisions that Torque makes before the host renderer/material/skinning layer takes over.
