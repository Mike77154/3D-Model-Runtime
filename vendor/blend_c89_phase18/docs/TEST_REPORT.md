# Phase 02 — Test report

Date: 2026-11-02. Platform: Linux GCC C89 build.

**All regression tests passed** (`python3 tests/test_synthetic.py`), comprising:
- 8 header/architecture matrix fixtures: pointer width 4/8, legacy/extended headers, LE/BE, encoded versions 2.49, 2.79, 2.80, 3.05, 4.04, 4.05, 5.00, 5.02.
- 1 old-style synthetic MVert/MFace mesh extraction route (run for each matrix fixture).
- 1 simplified synthetic Blender 5-like AttributeStorage array fixture.
- 1 standalone CLI schema/blocks group and 4 rejects for bad magic/truncation/compression wrappers.
- 1 multi-mesh test with 2 meshes and global OBJ indices.
- 1 interior pointer resolution test (saved address +4).
- 1 corrupted face index/rejected OBJ output test.
- 1 MPoly/MLoop + MLoopUV triangulation/UV test.
- 1 MPoly/MLoop + CD_PROP_FLOAT2 UV test.
- 1 5.1-like synthetic layout test with correct enum/array-size fields.
- 2 invalid modern attributes rejected (single storage/oversized array).
- 120 mutated-input process smoke probes: no process crash. Not a full fuzzer.

These tests do NOT use genuine `.blend` files created by Blender. There are no validated performance metrics or release-to-release compatibility results. Windows MinGW32 compilation was not performed here.
