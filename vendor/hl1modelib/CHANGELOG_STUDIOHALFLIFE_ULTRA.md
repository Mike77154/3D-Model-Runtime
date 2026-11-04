# Changelog - StudioHalfLife Ultra

## Ultra pass

Added:

- `hlfp_studiohalflife_ultra.h/.c`
- `hlfp_softmdl_ultra.h/.c`
- `test_studiohalflife_ultra.c`
- `demo_render_studiohalflife_ultra.c`
- `README_STUDIOHALFLIFE_ULTRA.md`
- `SOURCE_SCAN_STUDIOHALFLIFE_ULTRA.md`
- `BUILD_RESULT_STUDIOHALFLIFE_ULTRA.txt`
- `POLICY_SCAN_STUDIOHALFLIFE_ULTRA.txt`

New capabilities:

- Body/skin resolvers
- Attachment world matrices
- Animation driver with autolayer stack
- Root motion application
- IK rule evaluation over chains and direct targets
- Deep audit report
- Ultra QC supplemental writer
- Companion manifest writer
- Software debug sheet with checker, model, skeleton, hitboxes, and attachments

Validation:

- Built with `gcc -std=c89 -Wall -Wextra -pedantic -O2`
- Passed previous tests plus `test_studiohalflife_ultra`
- Generated `studiohalflife_ultra_preview.tga` and PNG preview copy
