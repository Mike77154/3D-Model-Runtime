# Third-party static decoder libraries

The files `linux-x86_64/libz.a` and `linux-x86_64/libzstd.a` are copies of
Debian's precompiled static zlib and Zstandard development libraries available
in the build environment (November 2026). These binaries are **not** C89-only
source and **not** CC0. Consult the accompanying Debian copyright notices.

- zlib 1.3.1 (zlib license), upstream: https://zlib.net/
- libzstd 1.5.7 (BSD-3-Clause or GPL-2.0 dual licensing), upstream: https://github.com/facebook/zstd

Our `src/bc_wrapper.c` uses a custom fixed-capacity allocator for zlib and
`ZSTD_initStaticDStream()` for Zstandard. Neither invokes heap allocators from
our codec source. The binary library archives still contain general-purpose
allocation routines, and stdio/temp files may allocate inside the C runtime.

Only the **Linux x86-64** static binary build is provided. For Windows/i686,
`build_win32_mingw32.bat` currently uses a C89 stub that rejects compressed
files, but retains the remaining decoder/rig features. Full Win32 vendor
sources are NOT included; do not claim full cross-platform compressed support.

The Blender SDK source archives supplied by the user were examined for schema
research, but are not redistributed in this package.
