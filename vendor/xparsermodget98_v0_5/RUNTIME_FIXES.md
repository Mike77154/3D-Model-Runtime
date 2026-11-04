# Runtime integration fixes

## Fixed

1. MSZIP reader state is now installed after `xpr_init()`, so `tzip`/`bzip` no longer lose their decompressor pointer and enable flag.
2. MSZIP inflation now has a 64 KiB contiguous history workspace: up to 32 KiB prior dictionary plus up to 32 KiB newly decoded data.
3. Dictionary refresh copies the last 32 KiB in one bounded operation instead of shifting the dictionary once per output byte.
4. Generic `.x` object references (`{ Name }`) are preserved through `on_data_reference`.
5. References inside `Animation` additionally emit `on_animation_target`, preserving the Frame/bone association for animation keys.
6. `AnimTicksPerSecond` is parsed and emitted through `on_anim_ticks_per_second`.
7. `AnimationOptions` is parsed and emitted through `on_animation_options`.
8. `XSkinMeshHeader` is parsed and emitted through `on_skin_mesh_header`.
9. `xp_fx32` is guaranteed to be a signed 32-bit fundamental integer type under C89. On LP64 hosts it selects `int`; on 32-bit Windows-style targets it can select `long`.
10. Building with `XP_ENABLE_MSZIP=0` now rejects compressed headers explicitly with `XP_ENOTSUP`.

## Runtime semantics

No humanoid skeleton is inferred. Frames, references, skin weights and animation targets are reported as authored in the `.x` stream. The caller/runtime remains responsible for resolving names into its own frame graph.

## Validation performed

- GCC strict C89: `-std=c89 -Wall -Wextra -Werror -pedantic`.
- Clang strict C89 with the same warning policy.
- Build with MSZIP enabled and disabled.
- Text and binary parser fixtures.
- Synthetic `tzip` and `bzip` regression fixtures.
- Two-block `tzip` regression with preset dictionary reuse.
- `sizeof(xp_fx32) == 4` on the LP64 Linux validation host.
- Source scan found no calls to `malloc`, `calloc`, `realloc` or `free`, and no C `float`/`double` declarations in `include/`, `src/` or `examples/`.


## Hardening pass v0.2.2

- Fixed `puff.c::fixed()` so Huffman backing arrays are wired before `construct()`, matching upstream zlib puff ordering.
- Consolidated `xp_mszip_stream` to one canonical declaration in `xp.h`; `xp_mszip_stream.h` is now the API/prototype compatibility header.
- MSZIP block reads distinguish clean EOF from truncated headers/payloads and reuse the shared `CK` signature checker.
- Removed the duplicate `on_mesh_begin`: it fires once before vertices (`nF == -1`), while the new `on_mesh_face_count` reports the later face count.
- Added integer multiplication/addition and unsigned-to-int guards before binary count arithmetic/callback casts.
- Unterminated braces, unknown-object blocks, binary child blocks, strings/GUIDs and partial binary tokens now fail as malformed instead of accepting EOF.
- Binary `TOKEN_STRING` terminators are validated as comma/semicolon, per the DirectX token record format.
- Text strings can use the full `XP_MAX_STRING` capacity; copies reject truncation instead of silently shortening values; control characters are rejected before string/path callbacks; backslashes are preserved instead of being dropped from Windows-style texture paths.
- `XP_ENABLE_TEXT=0`, `XP_ENABLE_BINARY=0`, and `XP_ENABLE_MSZIP=0` configurations compile cleanly under strict `-Werror` builds (all eight on/off combinations validated), and disabled source encodings are rejected by `xp_open_file()` with `XP_ENOTSUP`.

## Callback buffer lifetime

All pointers passed to callbacks are borrowed. String pointers, face-index arrays, matrices, animation value arrays, and similar buffers are valid only for the duration of the callback. A consumer that needs them later must copy them into caller-owned storage before returning. Texture filenames are data only: xparser does not resolve or open them.

### Hardening validation

- GCC and Clang strict C89 builds pass with `-Wall -Wextra -Werror -pedantic`.
- All eight `XP_ENABLE_TEXT` / `XP_ENABLE_BINARY` / `XP_ENABLE_MSZIP` toggle combinations compile.
- Original text, binary, tzip and bzip fixtures still parse.
- A synthetic MSZIP fixture forced to use a fixed-Huffman DEFLATE block parses successfully.
- Missing braces, partial binary tokens and invalid binary string terminators are rejected as `XP_EFORMAT`.
- Oversized binary Mesh counts are rejected as `XP_ELIMIT` before count arithmetic.
- A text texture path longer than 64 bytes retains its backslashes and reaches the callback intact.


## Corpus hardening v0.3

1. Text template declarations can contain square brackets without breaking the unknown-template skipper.
2. Mesh and MeshNormals index lists accept comma or semicolon separators used by legacy text exporters.
3. Binary `TOKEN_INTEGER_LIST` and `TOKEN_FLOAT_LIST` records are flattened into scalar lexer output, decoupling template grammar from writer packing choices.
4. Binary strings accept the WORD terminator used by real-world exporters while retaining compatibility with the older DWORD-style fixture encoding.
5. Standard `.x` MSZIP framing is decoded as total uncompressed file size plus repeated raw-size/packed-size/`CK` DEFLATE blocks.  Decoded block and stream sizes are validated exactly.
6. Legacy synthetic MSZIP framing remains accepted for backwards-compatible regression fixtures.
7. `TextureFilename` alone tolerates a horizontal TAB from old exporters that serialized a Windows `\t` sequence as control data.  Other strings remain strict.
8. `puff.c::fixed()` no longer owns mutable first-call static Huffman tables; each call builds its small fixed tables in automatic storage, removing the first-use race between decoder contexts.
9. Strict DEFLATE validation is retained.  No attempt is made to fabricate missing bytes from corrupt compressed assets.

### v0.3 validation

- GCC and Clang both compile all eight `XP_ENABLE_TEXT` / `XP_ENABLE_BINARY` / `XP_ENABLE_MSZIP` on/off combinations with `-std=c89 -Wall -Wextra -Werror -pedantic`.
- Source scan: no calls to `malloc`, `calloc`, `realloc` or `free`; no C `float`/`double` declarations; no `long long`.
- Seven packaged regression fixtures parse: the original four plus binary packed-list/WORD-string and standard tzip/bzip framing fixtures.
- External corpus: 196/199 parse (104/104 txt, 3/3 bin, 89/92 bzip).
- Replacement `xandy.zip`: 36/36 parse.
- UBSan completed the full 199-file corpus with no sanitizer finding; the same three malformed compressed files were cleanly rejected.
- ASan+UBSan completed the four original fixtures, three new v0.3 fixtures, a size-stress sample from the corpus, and all three malformed inputs without a sanitizer finding.

## Runtime provider / extension pass v0.4

1. The decoding pipeline is provider-driven through `xp_io`; FILE I/O is now an adapter instead of a core dependency.
2. MSZIP consumes the same provider stream, so `tzip`/`bzip` work from memory or a custom VFS without seek support.
3. `xp_io_init_memory` provides an allocation-free memory source. Regression tests also use a deliberately fragmented 3-byte provider to prove partial-read handling.
4. `xp_limits` adds runtime-selectable work budgets. Zero preserves the unlimited v0.3 behavior.
5. `xp_usage` exposes consumed object/model counters and decoded bytes.
6. `xp_diagnostic` adds source encoding, decoded offset and current object context while retaining legacy `on_error`.
7. Unknown/custom objects can be streamed semantically through `on_unknown_object_begin`, `on_unknown_token`, and `on_unknown_object_end` instead of being silently discarded.
8. Binary `template` declarations participate in the same unknown passthrough path as text declarations.
9. `VertexDuplicationIndices` is decoded explicitly.
10. `DeclData` and `VertexElement` are decoded explicitly; vertex declaration payload DWORDs remain raw.
11. `FVFData` is decoded explicitly; FVF payload DWORDs remain raw.
12. Text DWORD parsing now supports the full unsigned-int range instead of routing through signed `int`.

### v0.4 validation

- Frozen v0.3 external-corpus score preserved: 196/199, with exactly the same three malformed bzip sources rejected.
- GCC and Clang compile all eight feature-toggle combinations under strict C89 / `-Werror -pedantic` (16/16 builds).
- v0.4 extension fixtures pass in both text and binary encodings.
- `xp_open_io` passes with normal memory reads and a provider intentionally limited to three bytes per call.
- standard bzip parses directly from the memory provider.
- budget exhaustion returns `XP_ELIMIT` and emits a structured diagnostic.
- UBSan completed all 199 corpus files with no undefined-behavior finding.
- ASan+UBSan completed the v0.4 provider/extensions/budget regression harness with no finding.
- source scan found no allocation calls, C `float`/`double`, or `long long` declarations in the implementation.
