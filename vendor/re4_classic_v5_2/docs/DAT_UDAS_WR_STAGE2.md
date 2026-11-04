# RE4 DAT/UDAS WR — Archive indexing preparation (v2.1)

## References (inspected from the user-supplied ZIPs)
- `JADERLINK_DATUDAS_TOOL-main`: `DATUDAS_EXTRACT/Dat.cs`, `Udas.cs`. Target: LE, PC2007, PS2, UHD, PS4, Switch (as declared by project).
- `RE4_DASYZ2_TOOL-main`: `EXTRACT/Dat.cpp`, `Udas.cpp`, `CheckYZ2.cpp`, `YZ2/YZ2.cpp`. Target: BE, GC/Wii/X360/PS3 (as declared by project), including E3 and some YZ2 content.
- Extraction tools were **analyzed only**; their C#, C++/CLI or YZ2 third-party code is **not** redistributed in this new module.

## Added API
```
re4_dat_read_indexed(source, endian, is_e3, arena, &archive)
re4_udas_read_parts(source, endian, &parts)
re4_udas_read_indexed(source, endian, is_e3, arena, &archive, &parts)
```

- DAT/MAP: count at +0; table starts at +16 (final) or +4 (explicit GC E3); `count` 32-bit offsets and `count` 4-character file extension labels.
- DAT entry lengths are inferred from the next entry start (last entry ends at the DAT buffer's declared size), so alignment padding may be present. Avoid treating padding as part of an embedded model's semantic format.
- UDAS: up to two 32-byte descriptors at +0x20; type 0 denotes embedded DAT; other non-sentinel type denotes a separate sound/end section; the offset and size of each block are validated.
- UDAS archive entry offsets are adjusted to the **whole UDAS input**, so `re4_archive_entry_span(udas_span, ...)` works directly.
- A recognized YZ2 header in the UDAS's DAT section produces `RE4_ERR_NEED_EXTERNAL_CODEC`. No YZ2 decoding is implemented by this stage.
- Other legacy `re4_dat_read_guess` / `re4_udas_read_guess` APIs remain unchanged for backwards compatibility, but they may misinterpret actual RE4 offset/extension tables. Prefer the new indexed readers for supported variants.

## Tests
- GCC strict C89 / `-pedantic`, no dynamically allocated heap, no float/double.
- Synthetic LE and BE DAT; explicit BE E3; truncated/invalid offsets; nested LE/BE UDAS with sound block; YZ2-header detection.
- Original selftests and GC/PS2 synthetic tests retained.

## Not yet tested
- No user-provided retail RE4 `.DAT`, `.MAP`, `.UDAS`, `.DAS`, `.DRS`, `.DECMP` examples were included with these *tool* ZIPs.
- E3 DRS/extra REL is not supported by the new DAT reader.
- YZ2 full decompression and repacking are not implemented in C89.
- No Win32 (MinGW) binary validation, and no complete model/textures verification from actual game archives.
- Some DAT containers contain aligned/padded data; inspect file signatures and sizes when extracting.
