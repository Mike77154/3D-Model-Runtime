# FragFormats R10 — Ghidra targeted GameStudio MDL7 investigation

Target: `FileTypeMDL_GS.dll` from the user-provided fragMOTION 1.3.2 bundle.
SHA256: `449b57d63da7bd7f5ce8acf20116aea07cb8744b242ddf20b668b51d770ac057`.
Tool: Ghidra 12.0.4, x86 32-bit, deliberately `-noanalysis` plus explicit function disassembly.

## Executed traces

`TraceMDL7R10.java` over the MDL7 candidate/import route `0x10010890`:

- **3,937** instructions enumerated.
- **224** direct calls to **73** distinct destinations.
- **222** indirect calls (these are NOT safely resolved from this pass).
- Called `0x1001d9e0` at 16 sites between `0x10010ff0` and `0x100111ac`.
- Called `0x1001d7d9` 24 times and `0x10018690` 22 times.

`DisassembleHelpersR10.java` analyzed three representative targets:

- `0x1001d9e0`: `FSTCW`, `FLDCW`, `FISTP qword ptr`, then returns EDX:EAX. It is an **x87 floating-point to integer conversion helper**, NOT evidence of a file-byte reader.
- `0x1001d7d9`: loops over elements and invokes a function pointer from a stack argument. This is consistent with a repeated callback dispatch, but its MDL7-specific role is not proven.
- `0x10018690`: clears three fields of an ECX-addressed structure and calls `0x1001db9b` when one field is nonzero; a possible object cleanup pathway, exact semantics pending.

**Do not infer MDL7 serialization from helper call counts alone.** No live execution of proprietary fragMOTION was performed. Ghidra did not establish native skinning order, bind offsets, or matrix convention.

## Implementation result R10 (experimental)

Independent C89 implementation adds opt-in `--skin-raw` for MDL7 geometry:

- Applies the selected frame's *raw* 4x4 bone matrices as row-vector affine transformations, weighted per vertex via Q16.16 linear blend skinning.
- Performs no inverse bind-pose multiplication and assumes no bone-space pre/post corrections. The result is only a **diagnostic preview**, NOT an authoritative reconstruction of GameStudio animation.
- Missing bones for a vertex's influences contribute identity transforms; zero-influence vertices remain unchanged.
- Rejects non-affine preview matrices, duplicated matrices for one bone in a frame, unknown bone IDs, unsupported preview bone count (>4096), malformed weights and weighted vertices whose weights do not sum to Q16.16 unity (tolerance 3 ULP).
- Uses static arrays; no `malloc`, `calloc`, `realloc`, `free`, C99 or floating point in the C program. The Python test fixture generator is separate and may use `float`.

## Reference material

- User-provided `SDK/Includes/MDL7.H` with packed `MD7_BONETRANS` / `MD7_DEFORMER` definitions.
- `assimp/assimp`'s `code/AssetLib/MDL/MDLLoader.cpp` (secondary implementation for contrast): the importer's `ParseBoneTrafoKeys_3DGS_MDL7` and `AddAnimationBoneTrafoKey_3DGS_MDL7` map matrix entries and emit position, scaling and rotation keys. This is independent evidence, NOT proof of native fragMOTION's full skinning convention.

## Reproduce targeted binary scan

```sh
GHIDRA_HOME=/path/to/ghidra_12.0.4_PUBLIC
"$GHIDRA_HOME/support/analyzeHeadless" /tmp/r10project GS_R10 \
  -import /path/to/FileTypeMDL_GS.dll -noanalysis \
  -scriptPath ghidra/scripts -postScript TraceMDL7R10.java /tmp/r10calls.txt -deleteProject
```

All log files are included in the accompanying Ghidra Analysis archive; neither fragMOTION nor its DLL are redistributed.
