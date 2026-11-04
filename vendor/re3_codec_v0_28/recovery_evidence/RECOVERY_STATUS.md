# Recovery status

The original v0.13 ZIP remains visible in the conversation metadata but its
backing object can no longer be materialized. Multiple recorded copies return
the same unavailable-backing condition. No old v0.13 filesystem worktree was
mounted in the current runtime and no connected repository contained it.

This package therefore prioritizes **source preservation without pretending**:

## Recovered with high confidence

- public API shape and v0.13 version
- 15-section RE3 EMD directory model
- EDD/EMR/MD2 decoding rules used by the v0.13 work
- Binary00 ownership/seam grammar
- Binary01 morph grammar and validation invariants
- TIM VRAM parser/sampler and 0x8000 correction
- raw `re3c_read_section()` API
- original Makefile and MinGW build structure recorded in the session
- historical corpus/test results and original ZIP SHA-256

## Reconstituted wrappers

Small CLI/test wrappers (`re3emd_info`, `re3emd_parts`, `re3emd_morph`, the
wireframe viewer, and synthetic tests) were reconstituted around the recovered
API because their old attachment bytes were not directly accessible. They are
strict-C89 buildable and intentionally simple.

## Do not do this

Do not rename this recovered archive to the old hash or claim it is byte-for-byte
the lost original. If the original attachment is ever restored, compare it to
this tree and use the recorded SHA to identify the genuine old ZIP.
