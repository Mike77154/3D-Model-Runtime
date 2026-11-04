# v0.18 — 2026-11-02

- El VFS recupera hasta tres texturas de etapas adicionales por superficie desde paquetes PK3 superpuestos, respetando la prioridad de montaje para cada imagen.
- El archivo `materials_NNN.tsv` pasa a 7 columnas (`surface`, `material`, `image`, `shader_local`, `stage1`, `stage2`, `stage3`); el renderer sigue aceptando mapas de 3 y 4 columnas.
- `material_sources_NNN.tsv` mantiene las columnas anteriores y añade ruta real, paquete de origen y estado para cada etapa adicional.
- `animMap` de etapas secundarias cambia de recurso por tiempo, vuelve a descomprimir cuando cambia el PK3/imagen e invalida el slot de la caché de texturas.
- Al indicar `SHADER_FILE @ruta/en/pk3.shader` se conserva localmente el shader elegido durante la secuencia para evaluar todas sus etapas.
- Pruebas sintéticas con texturas TGA y JPEG baseline, mods con nombres de rutas de distinta capitalización, imágenes ausentes, cuatro etapas, cambios temporales y rechazo de rutas peligrosas.
- Sin herramientas nuevas; se mantienen 17 CLI. No se han incorporado assets comerciales ni código GPL del motor original.
- Los efectos avanzados del shader siguen fuera del alcance de este renderer aproximado.

# v0.17 — 2026-11-02

- Nuevo analizador independiente de stages `rtcw_stagefx.c/.h`, CLI `rtcw_fxprobe` y límites explícitos.
- Rasterización de capas adicionales con mapas locales TGA/JPEG, selección de imágenes `animMap` por tiempo y composición en pasadas transparentes ordenadas.
- Transformaciones de coordenadas UV `tcMod scale/scroll` en Q12, multiplicación de color `rgbGen const` y alfa `alphaGen const` en 8 bits.
- Corrección preventiva de desbordamiento en muestreo UV: módulo Q12 antes del producto con las dimensiones de imagen.
- Documentación del alcance limitado para recursos VFS y directivas no reproducidas.
- Suite visual sintética nueva, pruebas GCC/Clang/UBSan y regresiones heredadas.

# v0.16 — 2026-11-02

- Añade `rtcw_transorder` para triángulos translúcidos de atrás hacia delante con cola fija y ordenación in-place.
- Añade `rtcw_shaderstyle` con primera etapa `blendFunc` y `alphaFunc`, sin copiar código GPL.
- Integra alpha/add/filter con rasterizador; la prueba opaca conserva Z-buffer y la transparente no lo escribe.
- Extiende el mapa VFS opcionalmente a 4 columnas (`surface`, `material`, `image`, `shader_local`), manteniendo lectura de 3 columnas.
- Nueva herramienta `rtcw_shaderprobe`, tests sintéticos v0.16 y compilación estricta.
- Limitación deliberada: varios stages, efectos complejos y sorting por píxel NO implementados.

# v0.15 — 2026-11-02

- Agrega `rtcw_texcache` con 4 slots RGBA fijos, LRU e invalidación cuando el VFS sobrescribe una textura extraída.
- Agrega resolución dependiente del tiempo de `animMap` (FPS decimal Q1000) sin multiplicación desbordante de 32 bits; nuevo `rtcw_matprobe`.
- Mejora VFS para reutilizar extracciones inmutables y conservar los scripts `.shader` que definen un material a través de varios fotogramas.
- Agrega alpha blending en dos pasadas y contadores `texture_cache.tsv`, incluyendo cache hits/misses/evictions y extracciones reutilizadas.
- Preserva rutas API antiguas de materiales y renderizador usando instante cero.
- Actualiza Makefile y BUILD_WINDOWS.bat; incluye tests sintéticos animados / alpha / invalidación.
- Limitaciones: sin ordenamiento total de transparencias, ni pipeline shader completo; modelos originales de monstruos/Win32 nativo pendientes.

---

# v0.14 — 2026-11-02

- NEW `rtcw_vfs_material.c/.h`: referencias de materiales por superficie, inspección de scripts `.shader` ganadores y selección efectiva de TGA/JPEG por prioridad multi-PK3. Guarda la procedencia por pose.
- NEW `rtcw_jpeg.c`: JPEG baseline secuencial (8-bit, 1/3 componentes, 4:4:4, 4:2:2 y 4:2:0), descompresión Huffman e IDCT de enteros, sin libs ni heap.
- NEW `rtcw_imagecheck_main.c`: herramienta CLI nº 14 para verificar TGA/JPEG y exportar PPM.
- `rtcw_texrender.c`: entrada `rtcw_render_textured_map`, con reporte de imágenes no encontradas.
- `rtcw_rig_main.c`: en `run-vfs`, las poses a color se renderizan automáticamente si no se indica `TEXTURE_ROOT`.
- Tests nuevos sin librerías Python adicionales; conserva visibilidad, geometría y animaciones anteriores.

---

# Changelog v0.13

- Adición: módulo independiente `rtcw_visibility` para asignar superficies y filtrar triángulos con la máscara de visibilidad de los comandos.
- Adición: `ROOT_PART`, `TEXTURE_ROOT`, `SHADER_FILE` y campo opcional `PART:N` en cada ATTACH.
- Adición: vistas `pose_NNN_color.ppm` y mosaicos texturizados `contact_color_NNN.ppm` mediante `rtcw_contact_color`.
- Integración: `rtcw_rig` enlaza renderizador TGA, materiales y texturas. En modo VFS se permite `SHADER_FILE @ruta` como candidato explícito.
- Diagnóstico: `hidden_triangles` y `missing_textures` en `timeline.tsv`.
- Corrección: se actualiza `BUILD_WINDOWS.bat` y Makefile para los módulos nuevos.
- Verificación: 24 comprobaciones nuevas, 13 suites completas, 13 programas compilados con GCC y Clang C89, Clang/UBSan para v0.12 y v0.13. **Solo corpus sintético**.

---

# v0.12 (2026-11-02)
- Added animated child interpolation and decoded-parent attachment functions (`rtcw_attach.c`).
- New independent `rtcw_rig` CLI for deterministic multi-part scenario pose sampling.
- New `rtcw_contact` C89 contact-sheet generator (4x3 thumbnails, multi-page output).
- Added explicit `@virtual` lookup for multipart meshes and skins in multi-PK3 VFS, with source provenance.
- Updated legacy attach and multiattach targets to link `rtcw_motion.c` now required by interpolation API.
- Added synthetic tests covering 2-level parenting, animation following, multi-PK3 overrides, safety and pagination.
- GCC/Clang C89 builds, previous 11 regression suites, and new 34 v0.12 checks pass; Clang UBSan passes.
- No tested commercial RTCW monster assets; original runtime semantics not claimed.

# RTCW Natsuki Codec change log

## v0.11 — deterministic animation state preview

- Added `rtcw_preview` CLI for offline scenario-driven state, route and event previews.
- Added independent MDS torso/legs clocks; Q12 frame fractions and last-N loop frames, without heap, float or 64-bit integer types.
- Added exact condition string selection and ambiguity errors, with explicit `APPLY` index for investigative inspection.
- Added OBJ/PPM snapshot sequences and action/timeline TSV reports; sound and visibility commands logged as metadata.
- Added `run-vfs` to extract corresponding model/CFG/script from mounted PK3 files with provenance reporting.
- Added 74 synthetic checks and a separate CC0 test fixture; all legacy suites passed.
- Confirmed strict C89 builds with GCC and Clang and Clang UBSan tests for v0.11.
- Still does not execute RTCW gameplay AI, condition evaluator, shader effects, or sound playback; native Win32 and commercial corpus tests remain pending.

See `V11_README.md` for scenario syntax and technical limits. Previous release history remains available in older packages.

## v0.19 — Auditoría automatizada de modelos

- Nuevo `rtcw_audit.c/.h` y `rtcw_audit_main.c`: QC estructural por modelo.
- `local`, `vfs` y `batch-vfs` para análisis individual y lotes multi-PK3.
- Informes TSV de superficies, UV, clips, pose por fotograma, anomalías, materiales y procedencia.
- Detección de triángulos degenerados, geometría incorrecta, jerarquías óseas inválidas,
  clips fuera de rango y referencias de material no resueltas.
- Directorios de salida por modelo, sin confiar en nombres internos del ZIP como rutas locales.
- Pruebas sintéticas con modelo corrupto, prioridad de mods y recuperación de materiales.
- 18 herramientas totales, sin binarios precompilados en el paquete de fuente.
