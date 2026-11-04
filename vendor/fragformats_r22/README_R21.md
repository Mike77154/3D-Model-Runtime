# FragFormats R21 — UGH structured forensics (C89)

## Provenance

This version is based on the user's FragFormats R19 sources, their fragMOTION 1.3.2 executable and SDK, and the R20 synthetic corpus. No genuine exported .ugh file was found in the supplied RAR. R20 SYN0/TLV payloads are NOT native UGH data.

## Verified evidence, with precise limits

The SDK defines:

* `streamHeader` (16 bytes): dataID[4], dataVersion (u32), dataStart (u32), numObjects (u32).
* `objectRecord` (28 bytes): offset, numAttr, attributes, extraData, firstChild, nextSibling, reserved (all u32).
* `propertyRecord` (8 bytes): propID and value (u32 each).
* `fragVertex` (32 bytes), with xyz, three weights, four bone IDs, two flags words.
* `fragTriangle` (124 bytes), with flags, 3 indices, material/group/smoothing ID, normals, UV, corner colors, UV group IDs and selection flag.

A **targeted fresh Ghidra 12.0.4** disassembly of the real `fragMOTION.exe` found:

* `0x0046AF24` recursively walks an object tree and dispatches on object type codes (see separate report).
* `0x0046B3BB`, for type `0x2E`, queries COM IID_IMesh and calls `IMesh::ReallocData` (vtable +0x58), then processes face and vertex fields.
* `0x0046BB87` reads face records with a version-dependent layout: 88 bytes of old serialized fields or 124 bytes in the newer variant. This does not imply old in-memory `fragTriangle` has size 88.
* `0x0046BC76` reads the 32-byte `fragVertex` fields.
* `0x0046B138`, type `0x38`, queries IMatrix4x4 and calls SetMatrix (+0x20).
* `0x0046B1BF`, type `0x3E`, queries IMeshKeyList, calls SetNumFrames (+0x30) and CreateFrame (+0x48), and feeds vertex arrays to the keyframes.

These findings are **concrete evidence of mesh/matrix/mesh-key deserialization support inside the application**. They are NOT an externally validated UGH grammar: the exact on-disk framing and per-object offsets cannot yet be checked without genuine native UGH output.

## New tools

* `ughstructures`: reads the SDK-labeled 16-byte header, checks whether a hypothetical contiguous array of 28-byte `objectRecord`s could start at `dataStart`, and reports candidate records as TSV. The table position is a **hypothesis**, not proven by the SDK alone.
* `ughresync`: fixed-storage, insertion/deletion-aware UGH byte comparator. Searches 256 bytes forward for a nontrivial 16-byte common anchor and emits `insert`, `delete`, `replace`, `edit_mixed`, or `opaque_replace_tail` ranges. Alignment is heuristic, not proof of data-field identities. On repetitive data and changes larger than 256 bytes, it can fall back to an opaque replacement.
* Existing `fraginspect` UGH report gains SDK candidate names, while retaining old `reserved_word_*` records for backward compatibility.

All added C source uses strict C89 without dynamic allocation, float/double, or third-party dependencies. The R21 test generator uses Python only to build controlled binary fixtures.

## Build and test

```sh
make CC=gcc
sh tests/run_tests_r21.sh
sh tests/run_tests_r19.sh
```

## Usage

```sh
./fraginspect native.ugh content_scan.tsv
./ughstructures native.ugh struct_candidates.tsv
./ughcompare before.ugh after.ugh raw_difference.tsv
./ughresync before.ugh after.ugh aligned_changes.tsv
```

Only `ughresync` and `ughstructures` are new. Nothing in R21 animates or extracts a native UGH character; do not claim the synthetic fixtures prove fragMOTION acceptance.

## Next evidence needed

Save **genuine UGH exports** (empty/one bone/triangle/one key) from fragMOTION 1.3.2 under Windows; compare pairs to determine whether the `streamHeader` and object records appear directly in those files and whether the object type dispatcher operates on unwrapped bytes or higher-level COM object streams. Once confirmed, build parsers for object hierarchy, IMesh geometry/skin, IMatrix4x4 transforms and IMeshKeyList clips, and validate them in a frame-rendering runtime.
