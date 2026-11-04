# Phase 13 — VTK direct capture from original MDL (2026-11-02)

- Added `gs3d_vtk_bridge.h` and `src/bridge/gs3d_vtk_bridge.c` read-only, bounds-checked C89 accessors for native geometry, UV, skins, bone weights, bone names and frame names.
- Added `tools/vtk_capture.py` to render real `.mdl` to PNG via `vtkPolyData` directly in memory. No OBJ/MTL/GLB/FBX interchange is used in the preview path.
- Added `tools/audit_real_corpus.py` for real user-supplied MDL diagnostics through the bridge (user files excluded from the source archive).
- Added `tests/test_phase13_bridge.c` to test guardrails and weight/face/UV access using existing synthetic fixtures.
- Tested bridge with C89 GCC and Clang. `GS3D_RUN_SANITIZER=1 CC=clang sh scripts/check_phase13.sh` passes ASan/UBSan. Phase12 regression rerun also passes.
- Captured six authentic static MDL7 models at two VTK camera angles, and three experimental animated snapshots. Animation of Tusker frame 30 is visibly distorted and NOT validated against Acknex.
- Existing exporters retained only for backward compatibility; **not used by the new VTK screenshot workflow**.
