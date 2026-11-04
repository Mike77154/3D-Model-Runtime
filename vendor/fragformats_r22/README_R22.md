# FragFormats C89 R22 — UGH: native importer traced and linked object walker

## Critical correction to R19–R21

A **fresh targeted Ghidra 12.0.4** analysis of the user's original `fragMOTION.exe` (SHA256 can be independently checked) establishes the **actual UGH import entry point `0x00469FA1`**: it is in the same plugin vtable position as the known UGH exporter `0x0046B621`. The function checks `ugh0`, reads a 16-byte outer wrapper, **seeks to the u32 at outer offset +4**, reads a **second** 16-byte SDK `streamHeader`, checks version `0x00010000..0x00010001`, seeks to the second header's `dataStart` (offset +8), reads a 28-byte `objectRecord`, and runs the object restoration stages.

**Do not treat the first UGH wrapper as a `streamHeader`:** R19–R21's `sdk_*_candidate` labels were speculative and are superseded by this native code path. This does not prove compatibility with actual UGH files; none was present in the original fragMOTION package, and there is no native UGH acceptance test in this release.

## Native program evidence

| Function | Evidence from `fragMOTION.exe` |
|---|---|
| `0x00469FA1` | Native UGH import entry; 195 instructions; verifies `ugh0`, reads nested streamHeader and initial objectRecord |
| `0x0046A220` | Recursive objectRecord walker; `firstChild` and `nextSibling` are **stream seek offsets** (not indices); reads attributes via `numAttr` × 8 bytes; tracks an object ID map |
| `0x0046A3C2` | First traversal of existing child hierarchy |
| `0x0046A774` | Creates objects by name via IUserObject vtable `+0x164` (`CreateChildByName`) |
| `0x0046A9C9` | Follows/links object references and recursively processes children |
| `0x0046AF24` | Restores object properties and dispatches to type-specific handlers |
| `0x0046B3BB` | Type `0x2E`: native `IMesh` restoration, including triangle/vertex reading routines |
| `0x0046B138` | Type `0x38`: 64 bytes of `IMatrix4x4` passed to `SetMatrix` |
| `0x0046B1BF` | Type `0x3E`: `IMeshKeyList` frames, including counts and frame data |
| `0x0046B621` | Native UGH export entry |

The minimum supported nested version is built by `0x0046C353` from two WORDs (`1`, `0`), yielding `0x00010000`; the maximum explicitly checked is `0x00010001`.

The `IUserObject` method name at `+0x164` was verified against the original SDK's vtable declaration, not guessed from naming. The import stages are confirmed to be called by the UGH import routine. Additional reference trace and disassembly are packaged separately.

## C89 implementations

`ughnativeprobe` — follows the two-level UGH wrapper, validates nested version and bounds, and extracts the first objectRecord's seven raw words. It **does not load a model**.

`ughtree` — follows root/firstChild/nextSibling **absolute stream offsets**, without recursion or dynamic memory; validates bounds, detects loops/revisits, reports object records, three object data DWORDs, propertyRecord pairs, and bounded type-specific raw payloads. Reports `IMesh` initial two data words; `IMatrix4x4` sixteen raw matrix DWORDs; `IMeshKeyList` first three DWORDs, including the possible frame count. `ughtree` writes a TSV report and optional Graphviz `.dot` hierarchy. It **does not yet apply matrices, convert skins, evaluate vertex weights or play clips**.

`ughgraph` — preserves the **old contiguous-table hypothesis** strictly as a differential/research tool. It compares 0-based and 1-based interpretations and is *not* the native linked-record walker. Use `ughtree` for the layout revealed by the importer.

Previously working vendors (ARG, DirectX X, DBO, GameStudio, XMDL diagnostics) were left intact.

### Commands

```
make CC=gcc
sh tests/run_tests_r22.sh
./ughnativeprobe model.ugh wrapper.tsv
./ughtree model.ugh objects.tsv objects.dot
./ughresync before.ugh after.ugh diff.tsv
```

The source code builds with `-std=c89 -pedantic -Wall -Wextra -Werror -O2`, no heap and no floating point. Tests are **synthetic byte fixtures**, designed to validate code behavior and error handling, not to demonstrate that the original executable accepts them.

## Not yet solved

1. No native `.ugh` corpus: without a real file the exact relationship among object IDs, class names, properties, texture references, skeletons and animation clips remains unverified.
2. `IMesh` geometry, `IMatrix4x4` raw matrices and `IMeshKeyList` data are located in the importer, but an independent end-to-end mesh and animation decoder is **not** complete.
3. A genuine UGH saved by fragMOTION with a mesh, a bone and a single key would allow exact byte-level validation and extend this tool from structural walking to extraction of weights, UV and clips.

## Reproducibility and provenance

Private user-provided original `fragMOTION.exe` was analyzed with installed Ghidra 12.0.4; the original executable and copyrighted SDK headers **are not bundled**. The Ghidra investigation zip includes scripts and derived short technical notes and traces. All R22 added source is self-contained C89.
