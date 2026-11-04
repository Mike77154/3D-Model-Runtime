# R7 Ghidra 12.0.4 — FileTypeMDL_GS.dll

## Evidence and provenance

- Investigated the user's `fragMOTION 1.3.2.rar` extraction.
- Target: `Plugins/FileTypeMDL_GS.dll`, PE32/x86, imagebase `0x10000000`.
- Used supplied Ghidra 12.0.4 headless installation, with documented Java scripts
  in the separate `FragFormats_R7_Ghidra_Analysis.zip`.
- SDK `MDL7.H` details the packed layout of MD7_BONE, MD7_DEFORMER (16 bytes),
  MD7_DEFORMERELEMENT (28 bytes), MD7_DEFORMERWEIGHT (8 bytes), MD7_FRAME,
  MD7_FRAMEVERTEX and MD7_BONETRANS. C89 implementation is an *independent*
  reader derived from these record descriptions, not copied plugin logic.

## Direct observations from Ghidra

1. The partial automatic analysis identified **839 internal functions** in the DLL.
2. String scanner found UI/registration strings for `MDL3`, `MDL4`, `MDL5`,
   `MDL7`, and `GameStudio A8`, consistent with R6. Xrefs to these addresses
   were **not** resolved to import handlers on this partial analysis.
3. The largest function in the analyzed map is `FUN_10010890` (approximately
   13,058 address bytes), which calls 73 internal functions according to the
   partial call graph. A full decompile timed out; **its role is unknown**.
4. `FUN_1000c600` decompiled partially and has 23 called functions, including
   `FUN_1000ac80`; an analysis warning reports incomplete type propagation.
   Neither is conclusively identified as MDL7's deformer parser.
5. `RegisterEventHandlers` is at `0x1000a660`, and the partial graph shows
   16 function calls. The DLL imports Win32 `ReadFile`, `CreateFileA`,
   `SetFilePointer` and `WriteFile`, with imported IAT references shown in
   `ScanGSCallsR7.txt`.

**Caveat:** The headless analysis hit the autoanalysis timeout (13–16 s per
file) to avoid excessive running time. Some functions and xrefs may be missing.
No claim is made to have located the actual native deformer reader by address.
The new vendor parses documented binary records; its deformer placement is
synthetically checked and awaits comparison with authentic weighted MDL7.

## Reproduction

```sh
/path/to/ghidra/support/analyzeHeadless /tmp/ghidra_proj GS_R7 \
  -import /path/to/FileTypeMDL_GS.dll \
  -analysisTimeoutPerFile 14 \
  -scriptPath /path/to/ghidra_scripts \
  -postScript ScanGSCallsR7.java /tmp/ScanGSCallsR7.txt \
  -deleteProject
```

Scripts: `ScanGSDeepR7.java`, `ScanGSFunctionsR7.java`,
`ScanGSCallsR7.java`. Full function-list and partial pseudocode are saved in
`ScanGSFunctionsR7.txt`. Analysis is reproducible against user's supplied DLL.

## What next

1. Run Ghidra with a persisted project and larger analysis allowance; resolve
   the registration callbacks and import file-reader dispatch paths.
2. Capture a small **authentic rigged MDL7** and compare block offset sequence,
   data size fields, frames and deformer weights with our parser.
3. Derive bind-pose and animation semantics for a separate C89 pose evaluator.
