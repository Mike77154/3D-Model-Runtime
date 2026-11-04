# Hardening notes

## Format facts used

The Torque DTS layout uses three separate little-endian data buffers for 32-bit, 16-bit, and 8-bit values. dts89 v3.3 now decodes those buffers into arena-owned, native-endian pool arrays before the higher-level shape cursor consumes them.

The Torque DTS layout stores node/default/ground rotations as `Quat16` in the 16-bit pool. dts89 v3.3 reads and writes those values through the 16-bit pool and preserves them in the existing API as the lower 16 bits of the previous raw-bit fields.

The Torque DSQ layout stores animation-only data in a single little-endian stream and begins with one 32-bit version word. The decoder accepts v24-v26 and the probe performs a full structural scan to classify animation-only candidates before decode.

## Probe design

`dts89_probe(...)` does not allocate from the caller arena. It only scans the supplied byte span and fills a `dts89_probe_result`.

- DTS probe: shallow structural probe using header, pool boundaries, major counts, and optional sequence count after the memory block.
- DSQ probe: structural scan through strings, tracks, sequence tables, bitsets, and triggers.
- TorqueScript probe: ASCII/text probe for common Torque shape sidecars.

## Integer-safety pass

The v3.3 hardening pass added checked multiplication for count-derived arena allocations and skip lengths in DSQ and probe code. The DTS decoder now caps major format counts before arena allocations and before copying repeated arrays.

## Deliberate fixed-purist behavior

Torque stores many values as binary32 payloads. dts89 preserves those as `dts89_u32` bit patterns and exposes integer-only Q16.16 conversion helpers. That keeps round-trip behavior stable without requiring host real-number arithmetic.

## Still conservative / worth doing later

- Add a dedicated DTS guard-check auditor that mirrors Torque's `GUARD` checkpoints more strictly.
- Build a corpus test runner for real-world `.dts`, `.dsq`, `materials.cs`, and constructor files.
- Add optional semantic unpackers for every v27+ packed vertex variant while keeping the raw block path intact.
- Keep v29 encrypted DTS as detect-only unless the caller explicitly supplies the matching engine key stream policy.
