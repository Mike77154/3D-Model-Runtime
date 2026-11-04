# FragFormats R12: targeted MDL7 reverse-engineering and hierarchical pose experiment

Date: 2026-11-02. Static x86 PE target: `FileTypeMDL_GS.dll`, SHA256
`449b57d63da7bd7f5ce8acf20116aea07cb8744b242ddf20b668b51d770ac057`.

## Actual Ghidra execution

A **real Ghidra 12.0.4 headless run** imported the user's original DLL with `-noanalysis`, manually disassembled known entry addresses, constructed function boundaries, and recorded instruction and direct-call information in `Ghidra_R12/results/trace.txt`.

- `0x10010890`: 3,937 instructions, 224 direct CALL instructions and 222 indirect calls in the target function.
- `0x100181e0`: 38 instructions. Iterates over 0x58-byte (88-byte) records via `0x10013e40`, calls `0x1001db9b` and clears fields at object offsets +4,+8,+12. Very likely record-container destruction/cleanup rather than animation blending.
- `0x10018690`: 22 instructions. Calls `0x1001db9b` and clears three fields; consistent with freeing/resetting an array/container.
- `0x1001d7d9`: 34 instructions. Iterates a fixed-stride array in reverse, invoking a callback through `CALL [EBP + 0x14]`, consistent with a generic destructor/walker; **not evidence of multiplying bone matrices**.
- `0x1001d9e0`: 15 instructions. x87 `FSTCW/FLDCW/FISTP qword` conversion helper (previously identified in R11).
- Other scanned helper targets `0x1001da84`, `0x1001db9b`, `0x100027a0` appear to be runtime allocation/cleanup/dispatching helpers, not confirmed pose solvers.

These addresses/semantics are limited to this particular x86 DLL build and represent disassembly, not symbolic source reconstruction.

## New R12 model: hierarchical rest-relative diagnostic

The R11 `--skin-pivot` applies each pose matrix independently to vertices about the absolute rest pivot of the bone. It is **insufficient** for models where an animated parent joint moves an unanimated child.

`--skin-hierarchy` adds a hypothesis-controlled row-vector pipeline. With absolute rest pivots `B_i`, parent index `parent_i`, column-stored 3x4 frame transformation `A_i` (converted to internal row layout), and row-vector transforms:

```
T_i = translate(B_i - B_parent)     // root: translate(B_i)
LocalPose_i = A_i * T_i
WorldPose_i = LocalPose_i * WorldPose_parent  // root: LocalPose_i
Deformed_i(p) = transform(WorldPose_i, p - B_i)
Pout = sum(weight_i * Deformed_i(p))
```

Missing frame transforms are identity; weights are accepted only when their per-vertex Q16.16 sum is within 3 ulps of 1. Unweighted vertices remain in their original position. Parent traversal uses a fixed-size explicit path and state arrays (no recursive call stack, heap, float, double, or long long). It supports parents declared after their children; cycles are rejected in R9's parser.

**Crucial limitation:** This is not a proof of the native MDL7 matrix convention or bind treatment. It is a diagnostic for reproducing and comparing behavior, based in part on `assimp/assimp` `code/AssetLib/MDL/MDLLoader.cpp` matrix storage handling. Matrix `A_i` may already include other transformations on genuine MDL7 variants; such cases require original skinned/animated assets to verify. No interpolation, native inverse-bind matrix evaluation, deformation of normals, or skin texture reconstruction is claimed.

## Synthetic fixtures and results

`tests/create_mdl7_r12.py` generates controlled files from earlier synthetic rigs:

- Two-joint root R90 + child R90: vertex0 `(0,0,0)` with root 0.75/child 0.25 -> `(-0.25,0.25,0)` under hierarchy, compared with R11 pivot-only `(0.25,0.25,0)`.
- Identity frame stays in bind pose.
- Root-only rotation propagates to its child; root-weighted vertex1 moves to `(0,1,0)`.
- Forward parent reference preserves exact OBJ after reordering bone records, deformer links and key indices.
- Duplicate transform for same bone, NaN matrix component, parent cycles, and conflicting CLI flags rejected.

GCC `-std=c89 -pedantic -Wall -Wextra -Werror -O2`: entire R4–R12 suite passed. Clang same flags: new R11 and R12 suites passed. Original 4 ARG samples still decode with expected vertex counts (31, 12, 344, 367).

## Reproduction

```
/path/to/ghidra_12.0.4_PUBLIC/support/analyzeHeadless /tmp/gs_r12 GS_R12 \
  -import /path/to/FileTypeMDL_GS.dll -noanalysis \
  -scriptPath Ghidra_R12/scripts \
  -postScript TargetedMDL7R12.java /tmp/r12_trace.txt
```

No proprietary DLL, original ARG assets or 3rd-party models are included in either source or research ZIP.
