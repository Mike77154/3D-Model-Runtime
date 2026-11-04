# v0.3.0 engine bridge notes

This pass focuses on getting hl2modelib closer to a real engine import layer.

## New engine-facing path

```text
SMD triangles -> hl2m_smd -> hl2m_engine_model -> renderer/debug OBJ
```

The new direct SMD builder makes a simple path for tools that already have SMD reference meshes:

```c
hl2m_decode_smd(bytes, size, &smd);
hl2m_build_engine_model_from_smd(&smd, &engine);
hl2m_engine_model_bounds(&engine, &bbmin, &bbmax);
hl2m_engine_model_write_obj(&engine, "debug", text, text_size, &used);
```

## Why this matters

A compiled Source model is split across `.mdl`, `.vvd`, and `.vtx`, but authoring often begins at `.qc` + `.smd`. v0.3.0 supports both sides:

```text
compiled import: MDL + VVD + VTX -> engine model
authoring import: SMD -> engine model
recipe import: QC -> command IR + structured tables
material import: VMT -> material descriptor
```

That lets a custom game engine choose either a compiled Source asset path or a source-authoring path.

## New structured QC captures

The QC parser still preserves generic commands, but now it also exposes side tables for common gameplay/rigging data:

```text
lods[]
attachments[]
qc_hitboxes[]
definebones[]
poseparameters[]
```

This is useful for engine import because the renderer may only need mesh data, while gameplay and animation systems may need attachments, hitboxes and pose parameter metadata.

## Not yet done

Full StudioMDL binary compilation is not complete. The remaining hard layers are:

- writing production `.mdl/.vvd/.vtx` binary output from arbitrary QC+SMD
- sequence/animation compilation into MDL animation blocks
- flex/VTA support
- `.phy` collision model support
- VTF texture decode
- full Source shader tree execution
- material replacement list writing for VTX LODs
