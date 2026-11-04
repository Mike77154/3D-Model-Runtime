# Phase 03 — Rig / vertex groups: design and limitations

Date: 2026-11-02

## Serialized graph, not a mesh-only blob

```text
.blend block table + DNA1
  ├── ME / Mesh
  │   ├── dvert -> MDeformVert[verts_num]  (legacy)
  │   └── vert_data or vdata -> CustomDataLayer(type=2)
  │                                  └── data -> MDeformVert[verts_num]
  │      each MDeformVert.dw -> MDeformWeight[totweight]
  │                                    ├── def_nr (group index)
  │                                    └── weight (binary32 on disk; Q16.16 emitted)
  ├── Mesh.vertex_group_names -> bDeformGroup linked list (newer)
  ├── OB / Object.data -> ME; Object.defbase -> bDeformGroup (legacy)
  └── AR / bArmature
      └── bonebase -> Bone.next / Bone.childbase linked tree
                  ├── name
                  ├── arm_head / arm_tail (rest armature-space)
                  └── head / tail (fallback; coordinate-space differs)
```

`bc_skin.c` and `bc_armature.c` deliberately do not share mutable state or depend on `bc_mesh_legacy.c` APIs. They use the underlying reader and SDNA field offsets. Bone hierarchy traversal is an iterative, bounded queue, and both linked lists reject cycles.

The emitted TSV is an intermediate representation, **not** a fully evaluated or bind-ready skeleton. Vertex group index is not automatically a bone index, and a Mesh can be instanced by multiple Objects with distinct legacy group lists. The current name fallback selects the first matching Object. Bone transforms, object transforms, constraints, dual quaternion blend and keyframe animation are all omitted.

## Files tested and provenance

All rig files were programmatically constructed with the accompanying Python `tests/test_rig_synthetic.py` and **were not created by Blender**. The suite includes a 32-bit-pointer classic header and a modern LargeBHead8 header containing nonzero 64-bit address high words.

Validation with authentic Blender historical files remains essential: in particular, this C89 SDNA implementation infers field packing via a limited ABI alignment model and rejects structs that do not fit inferred saved type sizes. Some historical builds, platform ABIs, primitive aliases, `CustomData` changes and Blender versioning steps require additional vendors.

## Runtime integration suggestion

1. Parse Mesh vertex positions and topology as before.
2. Export weight tuples `(vertex, group, Q16_weight)` separately.
3. Export Bone rest hierarchy and coordinates separately.
4. Later resolve matching group names to bone names **per Object instance**, not globally per Mesh.
5. Before animating, build rest matrices, inverse bind matrices and a deterministic normalized-weight strategy with all operations in fixed point.

## Hard limits

- File size / offsets: max 2 GiB - 1, even though serialized pointers may contain 64-bit addresses.
- Bone tree: max 8192 serialized Bone nodes per Armature.
- Group list: max 4096 `bDeformGroup` records; names clipped at 95 chars.
- Per vertex: max 65536 `MDeformWeight` items (always constrained to source block length).
- MDeformVert count: max 10 million, further restricted by source block size; large traversals can be costly.
- No heap in authored C code, but CRT `FILE*` may allocate internally.
