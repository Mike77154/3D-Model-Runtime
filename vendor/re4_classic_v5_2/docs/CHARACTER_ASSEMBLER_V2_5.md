# RE4 Classic Codec v2.5 — multipart character assembler

## Scope and test evidence

This update adds a C89, fixed-point, allocator-free **runtime-facing skeleton retarget adapter** and a standalone CLI for RE4 PC 2007 multipart PMD characters.

- New public API: `include/re4c3d/re4_character.h`
- Implementation: `animation/re4_character_assembler.c`
- Example: `examples/re4_character_pose.c`
- Synthetic tests: `tests/re4_character_test.c`
- Original v2.4 skeletal, FCV, PMD, GCA, DAT, PS2 and GC files retained.

A character has one master PMD/FCV plus optional pieces. Each piece has its own bones and inverse bind matrices. We associate the `born__NN` bones in the master with `born__NN`, `lh00__NN`, and `rh00__NN` in other PMDs. Unnamed descendants inherit the nearest mapped ancestor's transform; otherwise their rest transform is preserved. No bone-ID-by-array-position assumptions are made.

For every frame and corresponding master joint `j`, evaluate:

```
Delta[j] = MasterAnimatedWorld[j] * inverse(MasterRestWorld[j])
PartAnimatedWorld[i] = Delta[map(i)] * PartRestWorld[i]
```

Then invoke existing `re4_skel_skin_vertex` with **the part PMD's own inverse bind matrices**, NOT the master's. A piece that has no matching bone and no mapped ancestor retains its own bind pose. An explicit `RE4_ERR_NO_SPACE` is returned if the caller does not provide sufficient output space.

## CLI

```sh
make -j4 && make check
./re4_character_pose MAIN.pmd CLIP.fcv 6 assembled.obj HAIR.pmd LEFT_HAND.pmd RIGHT_HAND.pmd
./re4_character_pose MAIN.pmd - 0 rest.obj HAIR.pmd LEFT_HAND.pmd RIGHT_HAND.pmd
```

The CLI writes OBJ with one material namespace per part, and MTL references to the original TGA names. It has a fixed maximum of eight PMDs, four MiB input PMD per part, and a 32 MiB shared arena. These are CLI demonstration limits; the library itself operates on caller buffers. The game textures are not shipped in this source package.

## Real user-provided files (private validation; not bundled)

**Leon:** `pl0000.pmd + pl0001.pmd + lh00.pmd + rh00.pmd` with `leon_fcv28.fcv`. Full character exported at frames 0 and 6: 14,751 vertices, 54 textured VTK meshes. Direct named bone links for hair=3, hands=2 each.

**Ganado em10:** `em1000.pmd + em1001.pmd + em10lh00.pmd + em10rh00.pmd` with `ganado_fcv1.fcv`. Full character exported at frames 30 and 90: 3,729 vertices, 13 textured VTK meshes. Direct named links for head=34, hands=15 each.

Rest-pose reconstruction versus unmodified 2007 PMD float coordinates: Leon max absolute coordinate error 0.000324, Ganado 0.000337, attributable to Q16.16 quantization and skinning roundoff. Both character poses have visibly different joint positions across two real keyframes, including accessory vertices.

## Validation

- GCC `-std=c89 -Wall -Wextra -pedantic`: no warnings in clean build.
- `make check`: seven passing test executables.
- ASan+UBSan synthetic tests: pass.
- ASan+UBSan actual Leon and Ganado multipart pose exports: pass.
- Four static VTK screenshots, two multi-frame VTK GIFs and textured GLB proofs are available in a *separate* preview bundle generated from the owner's original resources.

## Known limitations

- FCV root-motion, IK, scale, SEQ events and face-specific channels remain outside v2.5. C89 joint retargeting is verified on the sampled Leon and em10 models only.
- The bone link currently understands `born__NN`, `lh00__NN`, and `rh00__NN`. Other named aliases require additional explicitly verified mapping rules.
- Accessory attachment uses rigid master-joint deltas and the part PMD's native skinning. Physics-based hair and cloth simulation are not included.
- Captures use VTK externally, not a native RE4 runtime renderer. The provided C89 API can be integrated into a host but is **not** yet linked against any particular version of Model Runtime.
- Win32/MSYS2 compilation has not been tested in this environment.
