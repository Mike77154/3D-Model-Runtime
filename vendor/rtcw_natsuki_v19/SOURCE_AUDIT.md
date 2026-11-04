## v0.17 — auditoría de shaders

Referencias revisadas de los ZIP entregados por el usuario: `RTCW-SP-master.zip` (`src/renderer/tr_shader.c`, funciones `ParseTexMod`, `ParseStage`) para sintaxis `tcMod scale/scroll`, `rgbGen const`, `alphaGen const`, `animMap` y `blendFunc`. Se implementó código nuevo con buffers estáticos y enteros; no se incorporó código GPL. La implementación no pretende equivalencia de pipeline GPU.

# v0.14 — auditoría complementaria

Se extendieron los decodificadores originales de la rama v0.13, no se incorporó texto fuente de RTCW GPL ni librerías de imágenes ajenas. Se preserva la licencia CC0 del código nuevo. El análisis del código fuente del renderer id Tech 3 sirve como referencia de comportamiento, pero el visor es independiente y no pretende replicar la GPU/IA del juego.

Cobertura nueva: búsqueda VFS por archivos `.shader`, lectura acotada de la primera imagen de `map`/`clampmap`/`animMap`, TGA/JPEG en PK3 sobrepuestos y fuente ganadora por material; las asociaciones por rutas/arquivos son heurísticas y no certifican que los modelos sean del mismo monstruo.

---

# Auditoría incremental v0.13

Referencias de contraste: `RTCW-SP-master.zip` aportado por el usuario, especialmente `src/game/bg_animation.c` (parseo de `showpart` y `hidepart`) y `src/renderer/tr_animation.c`; las fuentes posteriores de RTCW y RealRTCW consultadas antes se mantienen como referencia, sin copiar código de terceros.

Decisión de ingeniería: los índices de parte 0..7 del archivo de animación no se corresponden de manera demostrada con el número de adjunto en nuestro visor. En v0.13 la asociación es **explícita** (`PART:N`), para impedir inferencias incorrectas. Los triángulos se compactan solo tras completar el ensamblaje de toda la jerarquía, así que los tags para hijos de piezas invisibles permanecen válidos.

El render color reutiliza el parser limitado de shaders y las imágenes TGA de la v0.5. No incorpora licencias del motor ni datos originales del juego. Mantiene el requisito C89 sin memoria dinámica ni coma flotante.

---

# v0.12 audit addition
The available v0.11 source archive was used as the implementation base. Earlier provided `RTCW-SP-master.zip`, `RealRTCW-iortcw-migration.zip`, and `Wwasm-main.zip` were reference-source archives. This version does not import GPL engine routines verbatim. The new rig intentionally does not infer part names or AI state conditions. Original copyrighted game asset compatibility remains unverified, since no real monster PK3 model samples were supplied.

## v0.9 — actual wolfanim.cfg grammar

In the user-supplied RTCW-SP-master.zip, `src/game/bg_animation.c` function `BG_AnimParseAnimConfig` recognizes metadata, `version`, `skeletal`, `STARTANIMS`, `ENDANIMS`, named animations (version >1), fields `firstFrame numFrames loopFrames fps moveSpeed [animBlend] [priority]`, and `HEADFRAMES`. `src/cgame/cg_players.c` references `models/players/%s/wolfanim.cfg` and `wolfanim.script`. The v0.7 four-column `animation.cfg` scan alone would miss real named wolfanim entries. Code for v0.9 is an independently written fixed-buffer subset, not imported GPL code. `wolfanim.script` parsing, event/action semantics, and head animation adjustment are explicitly deferred.

# Source audit — v0.7

The v0.7 work starts with `RTCW_Natsuki_C89_CODEC_v06_SOURCE_ONLY` and keeps the source tree intact. Reference sources previously supplied by the user were retained as external research inputs (`RTCW-SP-master.zip`, `RealRTCW-iortcw-migration.zip`, `Wwasm-main.zip`), but are not copied into the distributable code.

The archive index feature was exercised against all three uploaded source archives. Each could be indexed successfully and each contained **zero** files ending in `.md3`, `.mdc` or `.mds`; these are reference-engine source releases, not the base game's commercial model corpus.

The v0.7 resolver does not assume directory proximity proves shared animation rigs or attachments. It collects candidate siblings and follows only material references demonstrable via model surfaces, `.skin` values and matching `.shader` definitions. A generated shader is a deliberately simplified inspection material, not a recreation of the original RTCW shader execution system.

### Next engineering objectives

1. Read *multiple* PK3 archives in explicit load-order with last-wins patch precedence and VFS case normalization, without losing archive provenance.
2. Parse RTCW animation/script declarations and validate named clips against model frame bounds and torso/legs layering.
3. Construct verified tag hierarchies from explicit model metadata rather than filename guesses.
4. Validate materials, UVs, bones, split parts, and frame controls with legally sourced original RTCW model samples.
5. Test Win32 MSYS2 mingw32 build and restore hardware or optimized software textured view.

## v0.8 source audit

- This version integrates the **existing independent** v0.6 PK3 parser and v0.7 family resolver. New overlay indexing, collision reports, and VFS glue were written independently as original C89 source; no upstream renderer source was copied.
- The priority ordering follows **explicit mount list order** (later wins). The exact implicit ordering of files used by every RTCW engine/mod build was not asserted; that requires checking its particular filesystem implementation.
- No actual enemy models are bundled. The three uploaded references are engine/source distributions only and were indexed as ZIPs to smoke-test multi-archive scanning.
- Important distinction: same-directory file discovery supplies *candidate* pieces, whereas material mappings are inferred from skin/shader textual references.

## v0.10 — animation script analysis (2026-11-02)

Sources checked: user archive `RTCW-SP-master.zip`, specifically `src/game/bg_animation.c` (`BG_AnimParseAnimScript`, `BG_ParseCommands`, `BG_ParseConditions`) and `src/cgame/cg_players.c` (script filename `models/players/%s/wolfanim.script`). The parser distinguishes `defines`, `animations`, `canned_animations`, `statechanges`, `events`, and body-part commands. Based on these observations we wrote a **new original** fixed-memory C89 scanner/parser, not a derivative copy of GPL code. The new parser is offline-only, cannot select conditions or execute game events, and refuses unsupported command parameters. Same-directory CFG/script/model associations from VFS are hypotheses and explicitly identified as such. Test input is synthetic and does not establish all game-specific grammar variants or enemy coverage.
