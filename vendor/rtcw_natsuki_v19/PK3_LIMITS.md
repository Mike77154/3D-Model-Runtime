**Nota v0.17:** El renderizado de una segunda y sucesivas etapas exige que sus texturas estén disponibles en una raíz local. La recuperación multi-PK3 de imágenes adicionales para cada etapa NO está implementada. El VFS continúa resolviendo la primera etapa según v0.16. Ver [V17_README.md](V17_README.md).

# v0.16 — Transparencia y etapas de shader

Se reconocen `blendFunc` `blend`, `add`, `filter` y tres pares OpenGL equivalentes, más `alphaFunc GT0/GE128/LT128` de la **primera etapa**. Las demás etapas y operaciones se señalan como no interpretadas. La cuarta columna opcional del mapa VFS contiene el archivo shader extraído, si se pudo conservar. El orden translúcido aproximado se calcula por centroide y no resuelve intersecciones por píxel.

# v0.14: límites nuevos de materiales e imágenes

- JPEG baseline secuencial de 8 bits solamente; no progresivo, CMYK, aritmético ni DRI con intervalos de restart; tamaño descomprimido máximo 1024×1024 píxeles, archivo JPEG máximo 8 MiB.
- Los scripts shader se procesan individualmente con un buffer máximo de 512 KiB; un shader corrupto, sobredimensionado o con una referencia de textura insegura puede hacer que la previsualización se rechace.
- La primera referencia simple `map`/`clampmap`/`animMap` se considera un material de vista previa, no una ejecución de todas las pasadas del shader. Búsqueda inversa por orden del índice VFS, no selección garantizada equivalente al renderer del juego.
- Las imágenes se extraen en la carpeta de salida NUEVA; se regeneran por pose, sin caché de materiales entre fotogramas (optimización pendiente).
- No se genera asignación implícita de cabezas, armas o partes; los tags y el mapeo PART:N continúan siendo explícitos.

---

# PK3 explorer safety and limitations (v0.6)

Supported: standard, single-volume ZIP/PK3; STORE and raw DEFLATE (stored blocks, fixed Huffman, dynamic Huffman), ZIP entry CRC32 validation, model/texture/skin/shader/config inventory, basic relative-path validation and unpack-to-folder.

Unsupported: ZIP64; split archives; encrypted entries; unusual ZIP compression methods; huge central directory; entries larger than 32 MiB compressed or raw. Individual paths limited to 240 bytes; max 25 000 entries. Archive size limited by C `long` file offsets (around 2 GiB in 32-bit runtimes). Large datasets may consume about 64 MiB static memory. This is deliberate, not a full-purpose ZIP utility.

Security: path traversal components and absolute ZIP entry names rejected; central and local name/flag/method consistency checked; CRC32 checked before output. Extract into a fresh trusted directory; this portable code does not harden pre-existing directory symlinks against hostile filesystem races. In `unpack` mode files already present are skipped and not overwritten. In `extract` mode the explicitly supplied destination file is intentionally written.

Material/monster fidelity: catalog only groups by directory and file type. It cannot recognize a monster by name or automatically choose its skin/animation from engine scripts. The existing `rtcw_texview` handles only TGA (not rendered JPEG/PCX) and a limited RTCW shader subset; its preview is not an exact id Tech 3 renderer reproduction.

## Added v0.8 virtual filesystem caps

`rtcw_vfs` overlays up to 16 archive files by explicit order, at most 32,768 distinct virtual file paths, with up to 65,536 logged override events. Each path is validated using the existing PK3 safe path rule. Identical casefolded paths within the *same* archive are rejected as ambiguous; replacements across archives are expected and reported. An invalid or corrupt winner does not automatically fall back to old content. This VFS does not infer the original engine's automatic mount order. See `V08_README.md`.
