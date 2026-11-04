# FragFormats C89 R19 — UGH deep forensic pass

R19 prioritizes native fragMOTION `.ugh` reverse engineering. Existing ARG, Blade3D XMDL and 3D GameStudio code remain unchanged except for the new shared CLI dispatch branch.

## New C89 components

- `vendor_fragmotion/ugh_forensics.c/.h`: validates `ugh0` at offset 0 and a 16-byte minimum header; streams the opaque payload, counts bytes, and reports raw header fields and possible printable ASCII/UTF-16LE strings. A string candidate is **not** a decoded model attribute. Nothing in this module infers meshes, skeletons or clips.
- `ughcompare.c`: streaming bytewise compare of two UGHs, emitting byte offsets and exclusive ends of up to 1,024 differing ranges. No heap allocation.
- `tests/run_tests_r19.sh`: controlled *synthetic* UGH files, one UTF-16LE change and rejection of invalid/truncated headers.

## Commands

```sh
make CC=gcc
sh tests/run_tests_r19.sh
./fraginspect model.ugh ugh_diagnostics.tsv
./ughcompare baseline.ugh modified.ugh differences.tsv
```

The inspector does **not** generate OBJ or reconstruct UGH internal data yet. Existing R18 `.arg` animated-clip sampling and materials remain as-is.

## Real reverse engineering result

R18 recorded `IUserObject` vtable offset `0x178` as unknown. R19 matched the COM IID at EXE VA `0x0061D570` to `IUserObject` in the supplied `fragMOTION.h`, and enumerated its C vtable: slot 94 at `+0x178` is **CopyToStream**. The UGH saver `0x0046B621` indeed invokes that method with a COM `IStream` and flags `0xFFFFFFFF`.

The inverse-like `CreateChildFromStream` is at `+0x170`, but the recovered function `0x00458091` that calls it is actually a **clipboard paste** handler, *not verified UGH import*. The full interpretation of UGH child records is still unknown. See the separate R19 Ghidra report and traces.

## Honest limits

No genuine `.ugh` model exists in the provided corpus, and the R19 tests are format-header probes only. Skeleton hierarchy, bone weights, UV, material IDs, textures, bind-pose and animation keys cannot yet be decoded from UGH by this source package. The source remains C89 strict, no `malloc`, no `float`, no `double` and no `long long` in added code.
