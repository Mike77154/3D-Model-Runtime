# R4 evidence and change log

- Sourced the original FileTypeXMDL.dll, FileTypeDBO.dll, four original ARG
  examples and SDK/Includes/arg.h from user's supplied RAR via libarchive.
- Repeated ARG model checks: anchor 31 vertices / 31 faces; locked 12 / 10;
  jarjar 344 / 694 with 25 bones and one animation; snowman 367 / 678 with
  seven bones and two animations.
- XMDL DLL is PE32. `objdump -d -Mintel` confirms comparisons at VA
  0x100057AD, 0x100057BE; constants `xmdl` and `0100` in image data.
  No native XMDL file was present in RAR, so binary layout still unknown.
- Implemented new vendor for DirectX X text/binary 0302/0303, 32/64-bit,
  including text lexer, binary token lexer, bounded scene parser and OBJ/TSV.
- Rejected malformed `.x`: truncated streams, impossible vertex count,
  invalid face indices, compressed MSZIP without a decompressor.
- Strict GCC, Clang C89 build checks, valid samples, malformed sample tests,
  independent `trimesh` OBJ structural loading and authentic ARG regression.
- Future: MSZIP in an isolated vendor, external material reference linking,
  transform/pose evaluation, authentic X fixture cross-validation and XMDL
  stream layout reconstruction from real files.
