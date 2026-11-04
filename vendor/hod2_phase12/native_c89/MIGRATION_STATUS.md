# Estado de migración Phase09 → C89

## Leyenda

- **Base**: lógica nativa funcional, pero no equivalencia integral del script.
- **Parcial**: solo una parte del comportamiento.
- **Pendiente**: no implementado nativamente.
- **Referente**: ya existía C89 y se reutilizó/compiló.

**Ninguno de los scripts Python completos tiene aún un port 1:1**. Este registro evita confundir un conjunto de primitivas C89 con una conversión total.

| Archivo de origen | Función principal | Destino nativo | Estado |
|---|---|---|---|
| `audit_catalog.py` | Auditoría de 86 tipos | `hod2_cli.c` (`audit`), `hod2_tables.c`, `hod2_pol.c` | Base: no reproduce todo el informe original |
| `export_all_86.py` | Generar 172 GLB completos | `hod2_glb.c`, `hod2_cli.c` (`character`, `charactertex`, `batch`) | Parcial: jerarquía rígida, un clip y TEX compatibles; no skinning de abdomen ni comprobación de 172 activos reales |
| `validate_complete.py` | Comprobar 172 GLB, texturas, skinning y canales | `glbcheck`, `pol`, `audit`, `tests/test_native.c` | Parcial: sin validación profunda de glTF/poses |
| `build_character_clips_c89.py` | Generar tabla estática de asociaciones | `../inherited/c89/hod2_character_clips.c/.h` | Referente: catálogo compilado; generador Python sigue como fuente |
| `preview_full.py` | Previsualizador/rasterización de personajes | Sin equivalente nativo | Pendiente |
| `inherited/tools/build_abdomen_glb.py` | GLB de piezas deformables, articulación | `character_draw_bone_part.c`, `hod2_extra_parts.c`, `hod2_glb.c` | Parcial: faltan bridges/skin completo |
| `inherited/tools/build_glb.py` | GLB de personaje, texturas y clips | `hod2_glb.c`, `hod2_tex.c`, `hod2_mot.c` | Parcial: ya arma jerarquía rígida y un clip animado; faltan piel deformable, clips múltiples y casos especiales |
| `inherited/tools/export_all.py` | Exportación a escala | `hod2_cli.c` (`batch`) | Parcial: 86 tipos por versión, 1 clip opcional por tipo, CSV de ausencias; no 172 personajes con skinning |
| `inherited/tools/extra_parts.py` | Estructuras y decodificación de piezas extra | `../inherited/c89/hod2_extra_parts.c`, `character_draw_bone_part.c` | Base: estructuras/vertices estándar, falta caso especial |
| `inherited/tools/hod2_ui_export.py` | Decodificación y exportación con interfaz | `hod2_binary.c`, `hod2_pol.c`, `hod2_tex.c`, `hod2_glb.c` | Parcial: sin interfaz y sin flujo completo |
| `inherited/tools/inspect_models.py` | Tablas de ejecutables, modelos y movimiento | `hod2_tables.c`, `hod2_cli.c` (`pol`,`skeleton`,`clips`) | Base: no todas las inspecciones originales |
| `inherited/tools/render_validation.py` | Renderizar para inspección | Sin rasterizador C89 | Pendiente |
| `inherited/tools/validate_exports.py` | Auditoría de exportaciones | `glbcheck`, `tests/test_native.c` | Parcial |
| `ghidra_named_scripts/compare_extra_parts.py` | Comparación entre EXE de piezas | `hod2_cli.c` (`parts`,`peprobe`) | Parcial: comparar salidas externamente |
| `ghidra_named_scripts/HOD2QuickProbe.java` | Script dentro de Ghidra | `hod2_cli.c` (`peprobe`) | Parcial: lectura estática sin API Ghidra |
| `ghidra_named_scripts/HOD2TargetDecompiler.java` | Decompilación de rutinas en Ghidra | Sin equivalente C89 decompilador | Pendiente |
| `ghidra_named_scripts/HOD2TargetDecompiler2000.java` | Decompilación para otra variante | Sin equivalente C89 decompilador | Pendiente |

### Componentes C89 reutilizados

- `inherited/c89/hod2_extra_parts.c/.h`: lector de piezas y límites ampliados a 31.
- `inherited/c89/hod2_resource_guard.c/.h`: comprobaciones de spans, finitud IEEE754 y MOT.
- `inherited/c89/hod2_character_clips.c/.h`: asociaciones de clips y niveles de procedencia.

### Próximo salto hacia equivalencia real

1. Extender el **árbol rígido incorporado en Phase08** a `CharacterDrawBonePart` y subpartes normales y extras, sin inventar uniones.
2. Implementar atributos JOINTS_0, WEIGHTS_0, inverseBindMatrices y bind pose exactamente; reparar las costuras solo con datos comprobados.
3. Convertir las muestras MOT verificadas a canales GLB de posición/rotación, con el orden de huesos y transformaciones correspondientes.
4. Portar al C89 los modos de daño/subpartes y el rasterizador de vista previa para el diagnóstico visual.
5. Generar y validar 86 × 2 GLB completos con los archivos originales aportados por el usuario; comparar estructura, UV, PNG, huesos, clip y regiones sin malla contra el exportador Python.

**Nota de procedencia:** el paquete aportado conserva reportes originales de 172 exportaciones, pero no los EXE/BIN originales; no son evidencia de haber ejecutado ahora el motor C89 sobre esos activos.

**Novedades y mediciones Phase08:** consultar [`PHASE08_CHANGELOG.md`](PHASE08_CHANGELOG.md). El ensamblaje rígido no garantiza que los abdómenes/mallas deformables estén presentes.

### Phase09 — corroboración con bancos originales (sin ejecutables)

- `hod2_corpus.c` y los modos CLI `corpus`/`corpusdiff`: implementados en C89.
- Se han leído y auditado 2,428 ficheros BIN procedentes de los dos ZIP proporcionados.
- Son 650 POL interpretables y 49 directorios MOT por edición; hay 7 casos
  pendientes por edición, reportados individualmente.
- Exportación GLB estática real de piezas individuales (`znjoe`, `znsam2`),
  comprobada por `trimesh` sin que ello implique torso/abdomen articulado.
- No se reconstruyeron aún matrices óseas, tablas TEX, despachadores ni skinning
  de caracteres completos por falta de ejecutables en estos ZIP.
