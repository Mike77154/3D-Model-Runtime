# RTCW Monster Codec v0.8 — Virtual File System (C89)

**Purpose:** overlay original RTCW/RealRTCW/mod PK3 files into a *deterministic* read-only virtual directory, then feed the winning assets to `rtcw_family` without manually unpacking everything. This is **not** the game's exact load-order implementation: the caller specifies archive priority explicitly.

## Mount list

Create a UTF-8/ASCII `mounts.txt`, one existing archive path per line. **Earlier = lower priority; later = higher priority.** Empty lines and lines beginning with `#` or `//` are ignored; leading/trailing spaces are trimmed. Relative archive paths are resolved relative to the process's **working directory**, not the text file.

```text
# Load low priority first
C:/RTCW/Main/pak0.pk3
C:/RTCW/Main/pak1.pk3
C:/RTCW/Main/mods/creatures.pk3
```

You decide this order; don't blindly assume that the game's automatic file search uses this exact ordering. Valid archives must be **single-volume, classic ZIP**, without ZIP64/encryption and using STORE or DEFLATE.

## Commands

```bat
rem Report one effective winning entry per virtual path
rtcw_vfs.exe index mounts.txt all_winners.tsv

rem Log every overridden virtual path and old/new archive
rtcw_vfs.exe conflicts mounts.txt overrides.tsv

rem Show active source and how many times it was replaced
rtcw_vfs.exe where mounts.txt models/monsters/example/body.mds

rem Extract exactly the active winner (does not overwrite an existing file)
rtcw_vfs.exe extract mounts.txt textures/monsters/skin.tga skin.tga

rem Copy all winning entries, preserving folders, to a NEW directory
rtcw_vfs.exe unpack mounts.txt staging all

rem Filter by known model/skin/CFG or texture/shader extensions
rtcw_vfs.exe unpack mounts.txt model_assets models
rtcw_vfs.exe unpack mounts.txt image_assets textures

rem Build an effective multi-PK3 catalog of model candidates
rtcw_family.exe index-vfs mounts.txt monster_index.tsv

rem Make a new creature working folder, with resolved skins/shaders/textures
rtcw_family.exe bundle-vfs mounts.txt models/monsters/example/body.mds selected_monster
```

Replace example virtual paths with paths shown in your inventory. **`bundle-vfs` additionally creates:** `manifest.tsv` (including `source_archive` rows), `clip_ranges.tsv`, `selected.obj`, `selected_report.txt`, `selected_wire.ppm`, `selected_texture.ppm`, and `resolved_firststage.shader`, plus selected related files. `same_directory_candidate` is *not* proof of anatomical attachment. Only assets referenced by discovered shader/skin data can be considered explicitly associated.

## Resolution rules

* An exact slash-separated virtual path is matched **case-insensitively for ASCII**; the spelling of the active archive entry is preserved in outputs. Invalid and unsafe ZIP paths are rejected.
* The **last** package in the mount list supplying that same path wins. A replacement in another package is logged; two entries with the same case-folded path inside **one** package cause a hard error.
* If a winning archive entry is corrupt (including CRC mismatch), the extraction **fails**. The VFS does **not** silently fall back to an obsolete older resource.
* Multiple versions of a shader script at the same path are overlaid as files; *distinct* shader scripts with different names are all passed to the existing material resolver. Advanced multi-stage/animated shaders remain out of scope.
* A model in the base PK3 can use an overriding texture, skin or animation CFG from a later mod. The family report records the source archive for extracted assets.
* No global renaming of meshes or guesswork about boss identity, animation semantics, tags or skeleton compatibility.

## Build and protocol

`make` for GCC or Clang on Unix, or `BUILD_WINDOWS.bat` for MSYS2 MinGW32. Builds **9 CLI tools** including new `rtcw_vfs`. Sources comply with **ISO C89**, fixed static buffers, no floating point (`float`/`double`), no heap allocation and no ZIP/TGA dependency libraries. Win32 cross-compilation/runtime still needs testing on the user's actual compiler setup.

In C:

```c
#include "rtcw_vfs.h"
rtcw_vfs_file hit;
if (rtcw_vfs_mounts("mounts.txt") &&
    rtcw_vfs_lookup("models/creature/body.mds", &hit)) {
    /* hit.entry contains original PK3 metadata; hit.mount is active source. */
    /* Use rtcw_vfs_extract(path, output) to verify CRC and write a file. */
}
```

## Limits and cautions

* Maximum **16** archives, **32 768 distinct winning virtual paths**, **65 536 override events**, path length **240 bytes per ZIP entry**; archive path length **511 bytes**. Maximum PK3 records per archive **25 000** as inherited from v0.6. Limit rejection is deliberate; we do not silently truncate.
* Each extracted entry limited to **32 MiB** input and **32 MiB** output, using the inherited raw DEFLATE decoder. Files larger than 32 MiB can appear in the *inventory* but cannot be extracted; e.g. some reference-source archives may include big files.
* The VFS index is static resident memory (roughly 10–15 MiB), plus PK3 decoder's ~64 MiB buffers and the model/render buffers. Designed for desktop, not low-RAM microcontrollers.
* Extract only inside a **new trusted directory**. The portable directory creation helper does not defend against pre-existing symlink/reparse-point races or hostile changes by concurrent processes. Do not run this on untrusted archives with elevated privileges.
* A source archive's **relative path** is resolved from the *current working directory*; `mounts.txt` must remain unchanged between indexing and extraction. For permanent integrations, use stable full paths.
* No direct graphical realtime preview is included: produces OBJ + PPM inspection frames. TGA is decoded; JPEG and PCX can be catalogued/extracted, not previewed by this renderer.
* Some original models, modded shaders, compressed formats and commercial enemy animations are not yet empirically verified. All v0.8 tests use synthetic models. This source-only release contains no game assets or proprietary engine sources.

## Verification

```sh
make
python3 tests/test_codec_v08.py ./rtcw_vfs ./rtcw_family
python3 tests/test_codec_v07.py ./rtcw_family
# Also run all older test_codec*.py suites as documented in README.md
```

The three user-provided source repositories were scanned as ZIP references with the VFS: `RTCW-SP-master.zip` (647 records), `RealRTCW-iortcw-migration.zip` (2904 records), and `Wwasm-main.zip` (632 records). They contain no `.MD3`, `.MDC` or `.MDS` models: **they are code references, not a monster corpus**. See `TEST_RESULTS.md`.
