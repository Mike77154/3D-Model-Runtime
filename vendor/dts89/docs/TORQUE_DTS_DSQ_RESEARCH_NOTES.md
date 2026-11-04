# Torque DTS/DSQ research notes for dts89 v3.3-giffany

## Design constraints kept

- C89 source style.
- No internal heap ownership.
- Caller-owned arenas and caller-owned output buffers only.
- No native floating-point arithmetic in the library.
- Engine numeric payloads that are stored as IEEE-754 binary32 in Torque files are preserved as 32-bit words.
- Fixed-point accessors remain the editing/math layer.

## Official-format observations applied

### DTS

Torque DTS is the native binary shape container. The engine serializes a version/exporter word, then a memory buffer described in dwords and split into three logical pools:

```text
u32 version_and_exporter
u32 mem_buffer_dwords
u32 start16_dwords
u32 start8_dwords
u32 pool32[start16_dwords]
u16 pool16[(start8_dwords - start16_dwords) * 2]
u8  pool8[(mem_buffer_dwords - start8_dwords) * 4]
sequence tail
material-list tail
```

The v3.3 decoder copies those pools into the caller arena, endian-normalizes the 32-bit and 16-bit pools, and avoids treating file bytes as naturally aligned host structures.

Several rotation tracks are stored as quat16 values in the 16-bit pool. The public shape struct still exposes the existing `*_bits` arrays for compatibility, so the decoder stores the 16-bit components in the low 16 bits of each `dts89_u32`, and the encoder writes those low halves back through the 16-bit pool.

DTS v29 can use an XOR-obfuscated memory buffer in Torque3D. `dts89_probe` detects v29 and marks it as encrypted/unsupported; `dts89_decode_shape` rejects it instead of interpreting encrypted pools as plaintext.

### DSQ

Torque DSQ is animation-only binary data that can be shared by shapes with compatible skeletons. The DSQ decoder/writer keeps node names, tracks, sequence metadata, bitsets and triggers in caller-owned arena/output buffers.

The v3.3 DSQ decoder reads the single 32-bit version word and guards count fields, multiplication-by-3 track arrays, arena allocation sizes and trailing data checks before accepting a file.

### TorqueScript sidecars

Torque workflows frequently use `.cs` sidecars for materials and `TSShapeConstructor` behavior. The probe recognizes likely TorqueScript sidecars by text tokens such as `singleton`, `TSShapeConstructor`, `Material`, `addSequence` and `baseShape`.

## Probe status semantics

```text
DTS plaintext supported      -> DTS89_OK, kind DTS, SUPPORTED|BINARY
DTS v29 encrypted detected   -> DTS89_ERR_UNSUPPORTED, kind DTS, BINARY|ENCRYPTED
DSQ supported                -> DTS89_OK, kind DSQ, SUPPORTED|BINARY|ANIMATION_ONLY
TorqueScript sidecar         -> DTS89_OK, kind TORQUESCRIPT, SUPPORTED|TEXT
Unknown/corrupt              -> DTS89_ERR_FORMAT, kind UNKNOWN
```

## Still intentionally conservative

- The library does not claim to decrypt v29 assets because the exact key stream depends on engine configuration.
- Packed mesh payloads are preserved for round-trip workflows, but not every semantic editing path is decoded into a high-level mesh IR yet.
- Real-world corpus testing is still the best next validation step for odd exporter versions and game-specific asset conventions.
