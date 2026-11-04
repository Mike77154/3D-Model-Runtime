# R13 — MDL7 Ghidra and interpolation verification

## Target

`FileTypeMDL_GS.dll` supplied with fragMOTION 1.3.2. Reused the existing imported Ghidra project and executed `TargetedMDL7R13.java` using Ghidra 12.0.4 headless, without treating previous guessed function roles as facts.

### Verified helper routines

| VA | Direct observation | Confidence |
|---|---|---|
| `0x1001AD60` | Writes `0x3f800000` at offsets `0`, `0x14`, `0x28`, `0x3c` in an object at ECX; zeroes the other 12 dwords | High: identity 4x4 matrix initializer |
| `0x1001AC50` | `MOV ECX, 0x10`, then `REP MOVSD` from source to destination | High: copies 64 bytes (16 dwords), compatible with 4x4 matrix copy |
| `0x1001AA10` | Bounded NUL-terminating source string copy | High |
| `0x1001D3E0` | Forwards its argument to `0x1001DB9B` | High: wrapper only; `0x1001DB9B` appears to release memory, not independently proved here |
| `0x1001D9E0` | x87 control-word manipulation and `FISTP`, truncating floating point to integer | High |

`0x10010890` includes nine calls to the identity initializer, ten calls to the matrix copier, five to the string copy, six to the release-wrapper, and sixteen to the x87 integer conversion routine, according to direct callsite counting in the focused Ghidra report.

These facts **do not establish** the specific matrix multiplication order or whether the pose matrices are local/global, or the precise native interpolation rule.

### R13 change

Added diagnostic `--blend-next=<Q16>` for MDL7 (`Q16` integer in `1..65536`) with independent sparse frame-vertex overrides and componentwise fixed-point blend of consecutive frame matrices. It is *not* quaternion interpolation: blended rotation matrices may scale/shear. This diagnostic behavior is **not** claimed as a native GameStudio or fragMOTION feature.

If frame `N+1` doesn't exist, reject. Two-part pose blending (sparse vertices and bones) is tested; modes without the new flag are unchanged. No heap or native floating-point operations are used by the new code.

### Repro

```
make CC=gcc
make test
./fraginspect character.mdl pose.obj metadata.tsv --frame=0 --blend-next=32768
./fraginspect character.mdl pose_skin.obj metadata.tsv --frame=0 --blend-next=32768 --skin-hierarchy
```

Inputs tested: synthesized two-frame MDL7 (`2,3,4` to `7,8,9` on vertex #2), and synthesized two-joint MDL7 (animated parent + child). The test suite checks 25%/50%/100% factor, no-next-frame error, bad factors, duplicate options, hierarchy blend distinct from endpoints, and all R4-R12 regressions.

### Remaining limitations

No authoritative validation against original MDL7 *animated skeletal* assets. The native transform convention and normals are not yet fully reproduced. MDL2 remains probe-only; XMDL and UGH remain incomplete decoders.
