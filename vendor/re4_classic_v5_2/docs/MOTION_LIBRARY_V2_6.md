# RE4 Classic Codec v2.6 — FCV / SEQ / keyframes / FK poses / experimental IK

## Scope, measured

This is an incremental update of the existing C89 v2.5 source tree. The C library has no heap allocations, uses Q16.16 integers and remains independent of VTK, Python and the game's executables.

- New FCV encoding `0x80`: 1-byte signed value, 4-byte incoming and outgoing tangent; the previously rejected four Leon FCVs in `em/pl00.dat` now validate.
- Endian-aware FCV key-block and time reads; GameCube BE parsing has **synthetic** tests, not a full visual reproduction test.
- FCV 0xF0 type-15 corrected to 4-byte scalar value without tangents; maxFrame high two flag bits isolated.
- Lossless *source fields* in CSV (original frame IDs and raw numeric bit patterns, plus field widths), plus approximate Q16.16 numerical forms for each value and tangent.
- Union-of-key-times for every FCV and playback poses (world-space bone matrices) at every unique keyframe, using the main PMD skeleton. Pose data is **FK**; every unmatched/experimental IK/root/scale channel is accounted for but NOT automatically enforced.
- `re4_seq_timeline_read` decodes MotionSeqKey (10.6 frame time, sound ID, free flags) and batch exporter writes every sequence entry. These are not gameplay event trees or actual audio playback.
- `re4_skel_ik_ccd_two_bone` is an optional world-space CCD solver for supplied 3-joint chains and 3D targets. It passed a synthetic reachable-goal test, but it is NOT a native-RE4 IK emulator and is deliberately not applied automatically to exported keyframe poses.
- `ik_roots_NNNN.csv` identifies channels with IK-root flags (0x10/0x20), bend-axis code, four-joint/toe bits, and proposed joints/targets. `verified_game_chain=no` indicates these are heuristics, not confirmed game relations.
- Quaternions in PMD bind skeletons are now handled instead of rejected, enabling FK poses for em13 as well as Leon / em10. Tested with PMD rest-pose error of <=20 Q16 units (~0.000305 world units) for those three character bodies.

## How to run

```
make -j4 all
make check
./re4_gca_catalog em.dat --extract 'em\pl00.dat' pl00.dat
./re4_gca_catalog em.dat --extract 'em\em10.dat' em10.dat
./re4_gca_catalog xfile.dat --extract 'xfile\em\pl00\pl0000.pmd' pl0000.pmd
./re4_gca_catalog xfile.dat --extract 'xfile\em\em10\em1000.pmd' em1000.pmd
./re4_fcv_batch pl00.dat output/leon pl0000.pmd
./re4_fcv_batch em10.dat output/ganado em1000.pmd
```

To export **raw FCV curves and SEQ only**, omit PMD: `./re4_fcv_batch em17.dat output/em17`.

## Outputs by original DAT entry index

- `clips.csv`: one row per FCV: status, counts, maximum frame, key counts and special channel counts.
- `keys_NNNN.csv`: every key for each of 3 axes on every channel, the exact frame ID, value/tangents Q16.16, original raw scalar bits and width.
- `poses_NNNN.csv`: for every **unique** keyframe time, each PMD skeleton bone and its **12 world matrix Q16.16 components**. Root/IK/scale metadata is indicated by `special_unresolved`.
- `controls_NNNN.csv`: interpolation of special/IK/root/scale channels at the union keyframe times, *not* automatically applied to FK poses.
- `ik_roots_NNNN.csv`: candidate IK root info and unverified chain hypotheses for investigation.
- `sequences.csv` and `sequence_NNNN.csv`: indexed SEQ timeline keys in 10.6 fixed format.

A game motion is not equivalent to one FCV or one SEQ in all gameplay contexts: the runtime can combine archive clips, other archives, transitions, facial morphs and events.

## Caveats / open research

1. **Native IK**: exact game bend planes and lengths, heel-to-toe fixes, 4-joint chains, blending and end-effector target transforms need to be implemented and compared with a game-engine gold reference. The CCD overlay does not claim equivalence.
2. `re4_skel_evaluate` still applies only `bone_type==2` rotation channels automatically. Root position, root rotation, IK, scale and hybrid kinds remain in the exported controls rather than silently changing the character's world pose.
3. Keyframe matrices are FK **for the master PMD body**, not full mesh exports for every frame. For multipart characters, apply `re4_character_link` + `re4_character_retarget` to accessory PMDs as in v2.5, then skin vertices per requested frame.
4. Character shape keys, facial morphs and procedural eye movement are **not** fully reconstructed.
5. Per-platform animation formats and all game-wide event/weapon/room sources are not yet fully validated. Our corpus was supplied by the user (PC 2007) and not distributed within the full-source archive.
6. Windows Win32 build has not been run in this environment.
7. Q16.16 is a lossy *numerical conversion* of source float key values. CSV raw numeric bit patterns and widths preserve the source data for a later precision-preserving converter.

## Test evidence

- 8 C89 test suites pass.
- `re4_fcv_batch` validated on actual user-owned extracted RE4 PC 2007 archives. 65 FCV+23 SEQ in Leon `pl00`; 277 FCV+223 SEQ in `em10`; original keys and pose frames are fully exported.
- Additional `em11`–`em19` family tests were run; `em17` lacked the expected primary PMD for FK output but still produced all raw FCV/SEQ data.
- ASan/UBSan run against the original 342 FCV + 246 SEQ for Leon and em10 has been performed; corresponding final-code retest should be documented separately if any later changes occur.

### Source provenance

Implementation design uses clean-room format observations from the supplied source archives (including `re4-master/tools/motion/README.md`, the motion parsing reference and `JADERLINK` extraction tooling). No Capcom game code is copied into the library; no models or textures in the source archive.
