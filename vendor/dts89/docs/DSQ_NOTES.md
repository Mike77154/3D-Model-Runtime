# DSQ notes

This repo's DSQ support is intentionally narrow and practical.

Implemented reader/writer coverage:

- file header: Torque DSQ writes one 32-bit version word; high 16 bits are only preserved when present in legacy/library data
- node name table
- node rotation / translation / scale tracks
- arbitrary scale tracks
- ground transforms
- sequence table
- per-sequence bitsets
- triggers

The codec is designed for:

- importing external `.dsq` clips into fixed-buffer tooling
- exporting standalone `.dsq` files to pair with a base DTS
- emitting matching `TSShapeConstructor` glue scripts

The library does **not** try to interpret higher-level animation semantics beyond what the binary format directly stores.
