# v4.5 — FCV bone-mapping consistency and full-frame FK regression

## Actual fixes

1. Unified `re4_character_clip_coverage()` with the SAME `re4_skel_joint_for_fcv_id()` used in `re4_skel_evaluate()`. Removed the private outdated `born__NN` matcher. Previously this falsely marked crow/bat FCVs unmatched and could silently count `born__100` as `born__10`.
2. The native C89 mapper handles 2–3 digit skeleton IDs 00–255, including validated `em2300_crow__NN`, `em2900_bat__NN` prefixes; rejects malformed IDs, IDs above 255, unsupported namespaces, duplicates.
3. Added a native no-heap `re4_clip_sequence_audit MODEL.pmd CLIP.fcv [frame_limit]` tool to evaluate **every integer frame**, report FK evaluation errors, frame-count, changed frames, maximum whole-unit matrix component difference, saturation warnings and special channels ignored by FK. Fails closed on unlinked rotations, invalid clips or limits. Default max 2048 frames (opt-in 16384). `FK_ALL_FRAMES_OK` means **FK evaluated**, not native IK or mesh/texture correctness.
4. `examples/re4_v45_sequence_corpus.py` evaluates every frame of one previously FK-matching clip per modeled family and **all** 19 crow + 18 bat FCVs using user-provided archives; writes `sequence_results.csv` and SUMMARY.txt. No game bytes are shipped.

## Reproducible real-corpus results (this environment)

- 91 clips from 56 PMD families, 7,679 integer frames evaluated; 91 FK frame scans completed, no saturated matrix components in this sample.
- Crow: 19/19 FCV FK all frame scan; bat: 18/18 FCV FK all frame scan.
- 91 clips contain special channels that our FK engine **does not interpret natively**; do not conflate with full gameplay animation.
- 2 sampled clips (`em39`, `em4e`) had **no changing FK matrices** in their sampled two- or three-frame windows. This may be correct for their clip type, but requires semantic inspection. `em39` may also be a head-only master.
- Plaga A/B/C remains unresolved: `obm6600.pmd` is a research candidate, NOT verified A/B/C. `em25` is a legged Plaga; `obm6300` is tied to Krauser; `obm5500` appears to be Saddler's staff.
- Armadura A has no direct bone links between the selected PMDs; an explicit rigid anchor is still experimental and has not been assigned.

## Testing

`make clean && make all && make check` builds the strict C89 code; ASan/UBSan may be tested with an independent build. New `re4_large_fcv_joint_test` and updated `re4_coverage_test` check high numbers, animal aliases, ambiguous mappings. CLI also manually exercised with real data from user-provided xfile.dat and em.dat. Windows build and runtime integration remain unverified.
