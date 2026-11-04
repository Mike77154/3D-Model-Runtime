# RE4 classic codec v2.2: real GCAX / nested DAT corpus

## Important format finding

The six supplied files have `.dat` extensions but **are GCAX archives**:

- 0x00: `GCAX` (4 bytes)
- 0x08: 64-bit little-endian GCA3 footer offset (all six have zero high 32 bits)
- 0x10: start of contiguous stored entry payloads
- footer offset: `GCA3`, header 32 bytes, 32-byte records, and length-prefixed path names
- GCA3 record: flags at +4, uncompressed size u64 at +16, stored size u64 at +24
- File offsets are inferred **starting at 0x10 and summing the stored sizes in record order**.
- The stored sizes across *all* six samples sum exactly to `GCA3_offset - 16`.

This corrects v2.1's `re4_gca_read` bug: it had assumed payloads followed metadata at the END of the file, so it could not index these samples correctly.

## Real GCAX validation

| File | Bytes | Entries | Footer offset | Footer bytes | Status |
|---|---:|---:|---:|---:|---|
| st1.dat | 108039392 | 58 | 0x06708254 | 2700 | `RE4_OK` |
| st2.dat | 142126456 | 84 | 0x08789E40 | 3896 | `RE4_OK` |
| st3.dat | 127998428 | 84 | 0x07A10AA4 | 3896 | `RE4_OK` |
| st4.dat | 46403920 | 28 | 0x02C40C28 | 1320 | `RE4_OK` |
| st5.dat | 134003108 | 74 | 0x07FCAC38 | 3436 | `RE4_OK` |
| ss.dat | 222728887 | 1123 | 0x0D45B2B6 | 57345 | `RE4_OK` |

Total: 1,451 GCAX entries. `ss.dat` alone: 228 BIN, 552 TPL, 340 DAT, two TXT, one SND (includes empty entries).

163 room DATs inside st1..st5 were successfully parsed as little-endian indexed DATs. All of them have valid, ascending entry offsets. In ss.dat another 302 DATs matched the same table layout, 37 were zero-length and one did not pass the generic DAT-layout probe. 12,178 nested table entries were indexed across successfully read DATs, but those are NOT all standalone 3D models.

Observed nested extension counts: FCV 1,118; SEQ 174; BIN 957; TPL 758; SMD 162; SMX 163. Counts include zero-length or otherwise unvalidated assets where applicable and should be treated as inventory, not render success.

## Real model / texture spot check

From the user-provided `ss.dat`, *locally* extracted:

- `ss\\item\\cap01.bin` (332,224 bytes): `re4_probe` returned `RE4_OK` using the PS2-structure BIN reader, reporting 13 materials, 2 declared bones, 27 textures and enumerating vertex segments.
- `ss\\item\\cap01.tpl` (46,112 bytes): `re4_ps2_tpl_read` returned `RE4_OK`, reported 27 images, and `re4_ps2tpl_smoke` successfully decoded 27 / 27 images to caller-owned RGBA buffers without error.
- `st1\\r100.dat` (5,214,368 bytes): `re4_dat_read_indexed` returned `RE4_OK` with 49 entries. The initial entries include CAM, SAT, LIT, SMD, SMX, EFF and TEX.

These checks prove parsing and RGBA buffer decoding on selected real resources. They DO NOT prove correct UV placement on a rendered mesh, full scene import, skinning/animation, or coverage of all 3D model / texture formats. The packaged source contains NO copyrighted game assets.

## New API / examples

- `re4_gca_read_footer(footer_span, archive_size, footer_offset, arena, out)` reads ONLY the GCA3 metadata bytes. `entries[i].offset` is an absolute offset in the original GCAX file. The footer memory must stay alive while using entry names.
- `re4_gca_read(full_archive_span, arena, out)` now supports real GCAX data-before-footer and also preserves raw GCA3 header-first mode.
- `re4_gca_catalog`: portable C89 command-line example using `FILE *` + fixed buffers: indexes a large archive and optionally extracts a single named entry to an EXPLICIT user-specified output path.
- `re4_ps2tpl_smoke`: test a PS2 TPL's actual image RGBA decodability.
- `re4_probe` now tries the indexed LE DAT layout for nested `.dat` files instead of the legacy guessed layout. For large GCAX `.dat`, use `re4_gca_catalog` rather than `re4_probe`, whose sample input buffer is capped at 32 MB.

### Example commands (Unix shell; Win32 path spelling differs)

```sh
make clean && make && make check
./re4_gca_catalog /path/to/ss.dat
./re4_gca_catalog /path/to/ss.dat --list
./re4_gca_catalog /path/to/ss.dat --extract 'ss\item\cap01.tpl' cap01.tpl
./re4_ps2tpl_smoke cap01.tpl
./re4_probe /path/to/extracted/r100.dat
```

`re4_gca_catalog` never trusts an archive-supplied name as the output filesystem path. The user explicitly chooses the destination. The underlying library uses no heap allocation or floating point, and does not perform its own file I/O.

## Tests and limits

- `gcc -std=c89 -Wall -Wextra -pedantic`: completed without diagnostics in the checked build.
- `make check`: existing selftest, GC/PS2 synthetic tests, indexed DAT tests, and new GCAX footer/malformed-input tests all passed.
- `-fsanitize=address,undefined`: same four test executables passed in the sanitizer build.
- `re4_gca_catalog` successfully indexed all 6 real archives.
- Real texture decoding: sampled `cap01.tpl` decoded 27/27 images.
- Windows/MSYS2 build: **not yet tested**.
- Texture orientation, actual rendering, morphing, full mesh/UV correspondence, and skeleton animation remain to be validated visually.
- GCA3 offsets and `re4_archive_entry` fields use 32-bit sizes. This implementation is not for archives over 4 GiB.

## File inventories

The source ZIP includes `docs/real_gca_inventory.csv` (1,451 GCAX entries) and `docs/real_nested_dat_inventory.csv` (12,178 indexed entries), plus `docs/real_dat_sha256.txt` to identify the six user-supplied examples. The package does not contain original DAT, BIN, TPL, or extracted model files.
