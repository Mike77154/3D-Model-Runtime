# v1.0.20 — Source 2013 automatic pose layers and recursion

SDK inspected: `bone_setup.cpp` AddSequenceLayers/AccumulatePose; `studio.h` mstudioseqdesc_t/mstudioautolayer_t.

- NEW opt-in `hl2m_master_sequence_layers120`: traverses nested non-local, non-world autolayers up to depth 8, passing inherited weight and child cycle to grandchildren, detects cycles, reads per-bone masks and true DELTA children. Does NOT silently activate this in the old v119 API.
- NEW `hl2m_sequence_layer_envelope_control120`: STUDIO_AL_POSE selects its envelope with a *physical* (model-local) parameter; the child animation keeps its parent's cycle. Cycle-driven layers behave exactly as before.
- FIX: `posekeyindex` grid layout reads axis 1 at `groupsize[0]` stride but reads only `groupsize[1]` entries; checks actual binary bounds.

## Known divergences

This implementation is **not complete Source renderer/runtime parity**. Source resolves pose indices via `CStudioHdr::GetSharedPoseParameter` (virtual-model mapping) and uses a normalized `[0,1]` runtime parameter converted through `mstudioposeparamdesc_t`. Here `pose_values` are explicitly supplied **already in physical units**, by MDL-local pose ID. Caller maps/unwraps Source runtime poses. STUDIO_AL_LOCAL, STUDIO_WORLD, virtual sequences, special IK/context systems remain unsupported and cause explicit error. Nesting uses the local-sequence indices from supplied MDL, not virtual-model lookups. Source quaternion bone masks are approximated via existing fixed-point module; parity is incomplete. Cyclic dependencies are rejected, depth limited to 8. No malloc/float/double/64-bit integers in library.

## Validation

`make check` and `test_sequence_layers120` cover pose-driven envelope, non-pose equivalence, offset and layer safety, including real Birkin G1 corpus when files are supplied. The SDK is reference code, not executed Source engine; results are reconstructed locally.
