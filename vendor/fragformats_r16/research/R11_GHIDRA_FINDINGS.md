# FragFormats R11 — MDL7 targeted reverse engineering

Target: user-provided fragMOTION 1.3.2 `FileTypeMDL_GS.dll`

Tool: real Ghidra 12.0.4 PUBLIC 20260303, invoked headlessly with `-noanalysis`
and an explicit targeted disassembly script `TargetedMDL7R11.java`.

## Addresses supported by independent disassembly

| Address | Instructions | x87 opcode count | Observation |
|---|---:|---:|---|
| `0x1000C350` | 206 | 0 | Dispatcher candidate from previous route trace |
| `0x1000C600` | 1122 | 6 | Legacy GameStudio route candidate |
| `0x10010890` | 3937 | 89 | MDL7 import path from prior branch trace |
| `0x1001D9E0` | 15 | 4 | x87 conversion helper |

In the `0x10010890` function, `FLD`, `FMUL [0x1002E270]`,
`FADD [0x1002E410]` and `CALL 0x1001D9E0` repeat for four contiguous
variables at `0x10010FDE..0x1001103B`. The code then uses `SHL 8` and
`OR` to pack four results into a single 32-bit word by `0x10011054`.
The constants in the DLL's `.rdata` section are exactly IEEE754
`255.0` (0x437f0000) and `0.5` (0x3f000000), respectively.

This is substantial evidence of four-channel 8-bit quantization. The immediate
consumer could be a material/color parameter or a different four-channel
property. Do NOT claim an RGBA channel order without following the destination.

The separate matrix *storage* interpretation comes from comparing with Assimp's
MDL7 importer, not Ghidra proof of native fragMOTION pose evaluation:
`assimp/assimp`, `code/AssetLib/MDL/MDLLoader.cpp`,
`AddAnimationBoneTrafoKey_3DGS_MDL7`.
That importer maps MDL7's first 12 floats in successive column order.
Our `--skin-pivot` is an explicitly experimental, reproducible hypothesis.

## Reproduction

```
GHIDRA_HOME=/path/to/ghidra_12.0.4_PUBLIC
"$GHIDRA_HOME/support/analyzeHeadless" /tmp/ghidra_R11 GS_R11 \
  -import /path/to/FileTypeMDL_GS.dll -noanalysis \
  -scriptPath ghidra/scripts \
  -postScript TargetedMDL7R11.java /tmp/trace_r11.txt -deleteProject
```

Ghidra logs, trace and script included separately. Proprietary DLL omitted.
