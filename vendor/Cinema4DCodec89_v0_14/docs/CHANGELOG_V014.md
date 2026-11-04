# v0.14 modular partition

- Implemented five independently compiled modules: Animer, Shaderer, Mesher, UV Texturer, Texturer Opener.
- Relocated v0.13 implementations, no behavior change to original functions.
- Added five public module headers and five compact C89 adapter implementations.
- Centralized authoritative compilation list (`scripts/module_sources.sh`).
- New `build_module.sh` builds one module without rebuilding the other modules.
- Unified vendor build emits `libc4d89_core.a`, five module archives, and compatible `libc4d89.a` and `libc4d89_images.a`.
- Updated POSIX and MSYS2 scripts plus tests to use modular archives; no old absolute `src/c4d89_*` source lists remain.
- Added sample engine integration and module facade / legacy-image smoke tests.
- Existing resource, shader, texture, mesh and animation limitations explicitly preserved.
