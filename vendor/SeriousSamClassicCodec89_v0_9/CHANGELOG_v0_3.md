# v0.3 -- skeletal animation challenge

- New `include/ss89skin.h`, `src/ss89skin.c` : ISO C89 independent binary loaders for MESH11/12, SKEL6, ANIM14; sparse pose sampling, fixed-point quaternion NLERP, topological bone tree, bind-pose inverse and weighted CPU skinning.
- New `src/ss89pose.c` : CLI for skeletal pose export, animated OBJ sequences and fractional frames.
- First mesh UV layer preserved in posed OBJ.
- New synthetic test rig and source `tests/test_skinning.py`/`tests/test_skin_api.c`.
- Existing `ss89dump`, `ss89gro`, `ss89resource` retained without behavioral modification.
- Limitations and source references in `SKA_ANIMATION.md`.
