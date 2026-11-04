# RTCW Monster Codec v0.7 — automatic model-family asset preparation 🍓

**New in v0.7:** `rtcw_family` automatically indexes models inside a single RTCW `.pk3`, builds an isolated working folder for one selected model, follows **explicit** `.skin` / `.shader` / diffuse texture references, and creates a textured inspection preview. It conservatively places same-directory models, skins and CFGs beside the selected asset as **candidates**, without guessing their anatomical or animation relationship.

## Two commands

```bat
rem Scan PK3 central directory WITHOUT extracting anything
rtcw_family.exe index pak0.pk3 monsters.tsv

rem Use a NEW destination directory; do not create it beforehand
rtcw_family.exe bundle pak0.pk3 models/monsters/example/body.mds scan_001
```

The model argument must be an **exact internal PK3 path** from `monsters.tsv`. This is one archive at a time; combining multiple patch/mod PK3 files is intentionally deferred.

A successful bundle creates:

```text
scan_001/
├── manifest.tsv                 # actual sources, relation type, missing items
├── clip_ranges.tsv              # numeric 4-column CFG rows, if any
├── resolved_firststage.shader   # intentionally simplified preview-only materials
├── selected.obj                 # frame-0 mesh
├── selected_wire.ppm            # 800x800 wireframe
├── selected_texture.ppm         # 640x640 software textured view
├── selected_report.txt          # surfaces, tags, bones etc.
├── models/.../                   # selected model, same-folder candidates/skins/CFGs
├── scripts/.../                  # material scripts for dependency resolution
└── textures/.../                 # only referenced textures present in this PK3
```

The command does **not** construct an animated monster with all of its parts automatically. Its bundle prepares the candidate resources, leaves anatomy and tags to the `rtcw_multiattach` tool, and records where references are missing. The basic preview is of the selected model's **first frame only**, with `default.skin` if available (or a unique `.skin`). When multiple skins are ambiguous, no arbitrary one is applied for preview, but all sibling skins contribute candidates to the dependency search.

## What is explicit vs inferred

- **Direct evidence:** material name in a decoded surface, override from a `.skin`, shader block with a matching name, and `map` / `clampmap` / `animMap` referenced texture. Explicit referenced textures that do not exist in the same PK3 are listed as `missing_referenced_texture`.
- **Candidate only:** other models, skins and CFGs in the same directory. They could belong to different characters or variants. `rtcw_family` does not attach them without a known `tag_*` relationship.
- **Clip range candidate:** purely numeric CFG lines with `firstFrame numFrames loopFrames fps`. The table is only frame metadata; it is **not proof** that any named animation clip plays correctly on a particular model. RTCW animation scripts have more complex syntax and are not decoded here.
- **Simplified materials:** `resolved_firststage.shader` chooses one usable diffuse map per matching shader. `animMap` still lists every image as a dependency, but the still image uses its first map. No real-time shader stage blending, normal mapping, lighting or image animation is claimed.

## Supported / unsupported

**Supported:** model indexing in classic ZIP/PK3; stored and DEFLATE compression; CRC32 for extracted entries; MD3/MDC/MDS model probing; sibling collection; `.skin` override list; case-insensitive material-name matching; comments/quotes in basic shader tokenization; `map`, `clampmap`, `animMap`; direct texture names with `.tga/.jpg/.jpeg/.pcx` and extension candidates; numeric 4-column `.cfg` rows; previews via existing C89 renderer.

**Not yet supported:** transparent cross-PK3 overlay or patch precedence; cross-directory matching from model configuration scripts; automatic skeletal accessories; animation timeline names or play lists; `.jpg` and `.pcx` rendering (they may be extracted, TGA renders); all shader directives; text parsing of scripts larger than 1 MiB (logged and skipped); proper case-path virtualization on case-sensitive filesystems; original commercial monster QA.

**Limits:** MAX 32 MiB compressed + 32 MiB uncompressed **per PK3 entry** (inherited reader), 25,000 records, path <=240 bytes, max 384 same-folder models, 192 skins and 192 CFGs, 512 shader scripts, 1024 material names, 4096 texture-path candidates. If capacity is exceeded, the command rejects rather than truncating the result silently.

**Safety:** the destination **must not exist beforehand**; a new folder is created to avoid overwrites/symlink escape via preexisting contents. Extract only into a trusted parent, never run elevated on untrusted PK3 files. Output is not transactional: if an invalid or corrupted input is detected mid-extraction, remove the partially prepared output directory before retrying.

## Build

```sh
make                   # GCC / clang on Unix
python3 tests/test_codec_v07.py ./rtcw_family
```

```bat
BUILD_WINDOWS.bat      rem MSYS2 MinGW32 (requires GCC available on PATH)
```

All 8 tools are built from local, standalone C89 sources. No heap allocation, no `float`/`double`, no `stdin` and no third-party libraries. The C89 build and synthetic tests have been validated on Linux; **a native Windows/MSYS2 build is not claimed**.

## Relation to earlier versions

All v0.1–v0.6 code and executables remain; the new `rtcw_family.c`, `rtcw_family_main.c` and `rtcw_clipcfg.c` provide an optional preprocessing route. In a normal workflow:

1. `rtcw_family index ...` lists model paths.
2. `rtcw_family bundle ...` reconstructs candidate dependencies and frame-0 preview.
3. Inspect `manifest.tsv` and `clip_ranges.tsv`; confirm which pieces actually belong to your creature.
4. For tagged assembly, use `rtcw_multiattach`. For animated MDS inspection, use `rtcw_mds_anim`. The old viewers still accept extracted files.

Everything added in this version is original CC0 code. The contributed engine source archives were examined as references only; they are **not included**. No commercial models are packaged.
