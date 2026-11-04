# Ghidra 12.0.4 — GameStudio MDL import, R8

Target: `FileTypeMDL_GS.dll` from the user's `fragMOTION 1.3.2.rar`.
PE32 x86; image base `0x10000000`; file size 245760 bytes.
SHA256: `449b57d63da7bd7f5ce8acf20116aea07cb8744b242ddf20b668b51d770ac057`

## Verified decompiler evidence

- **0x1000C350** (`FUN_1000c350`) opens the model and reads its first four bytes.
  It compares the signature against three legacy MDL constants and dispatches
  those paths to **0x1000C600** (`FUN_1000c600`).
- When the MDL7 signature branch matches, it calls
  **0x10010890** (`FUN_10010890`). The branch is visible in
  `ScanGameStudioR8.txt`, not inferred merely from GUI metadata.
- **0x10013BD8**, labeled by Ghidra as an exception handler, contains a
  specific message for files not containing an MDL7 model.
- The SDK's `MDL7.H` documents variable MD7_FRAMEVERTEX strides, the
  `MD7_DEFORMER` -> `MD7_DEFORMERELEMENT` -> `MD7_DEFORMERWEIGHT` hierarchy,
  and the dual animation strategies (vertex frames / bone transforms).

## Scope and remaining unknowns

- 1327 functions identified in this longer run (R7 partial: 839).
- Automatic Ghidra analysis timed out at 65 seconds; script completed afterward.
- `FUN_10010890` is about 13058 address bytes and its **full body is not yet
  reliably decompiled**. We **cannot yet identify** the exact per-deformer
  native reader or confirm its position relative to group/frame records.
- The R8 C89 pose implementation uses SDK record interpretation and synthetic
  tests. It is not reverse-translated proprietary source code.
- Future investigative target: callers/callees of `FUN_10010890`, especially
  read loops with 16/28/8-byte records and handling of `deformerdata_size`.

## Reproduction

Unzip Ghidra 12.0.4; obtain target DLL from user's legitimate fragMOTION copy;
then run:

```sh
mkdir -p /tmp/ghidra_project
GHIDRA_HOME/support/analyzeHeadless /tmp/ghidra_project GS_R8 \
  -import FileTypeMDL_GS.dll -analysisTimeoutPerFile 65 \
  -scriptPath scripts \
  -postScript ScanGameStudioR8.java /tmp/ScanGameStudioR8.txt \
  -deleteProject
```

Do not redistribute the proprietary DLL or SDK in the source archive.
