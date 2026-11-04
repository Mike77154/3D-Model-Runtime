# xparsermodget98 v0.4

Parent: frozen v0.3 SHA-256 `e97241f954f15700944002ce3445d6a59dde57a7e92983eb3b1d28b9fbd88a9f`.

## Added

- provider I/O (`xp_io`), memory provider and stdio adapter
- parser budgets, usage telemetry and structured diagnostics
- unknown/custom object semantic passthrough
- VertexDuplicationIndices, DeclData/VertexElement and FVFData
- MaterialWrap and MeshFaceWraps
- stable Frame node IDs including anonymous/duplicate Frames
- mesh owner-frame node binding
- mesh/bone/inverse-bind skin callbacks
- DirectX fixed-point matrix/point helpers
- raw animation semantic normalization and AnimationSet summaries
- legacy raw keyType 4 + 16-value matrix compatibility
- DirectX-to-OpenGL UV V-flip helper that preserves tiling
- new rig/UV/animation regression fixture and test harness

## Runtime rule changes

- animation matrix keys are absolute local pose values, not delta multipliers
- skin final matrix is `matrixOffset * boneCombined` in the authored DirectX convention
- do not multiply the mesh owner Frame again after weighted skinning
- UVs are never clamped or normalized by the decoder
- omitted AnimTicksPerSecond remains unknown rather than receiving an invented rate

## Validation

- external corpus: 196/199 (same three malformed DEFLATE files as v0.3)
- txt 104/104, bin 3/3, bzip 89/92
- strict GCC+Clang feature matrix: 16/16
- UBSan full 199-file corpus: zero findings
- ASan+UBSan regression/real-model sample: zero findings
- protocol scan: no heap allocation calls, float/double declarations or long long
