# XPS geometry cache fix

The XPS parser reports a submesh in two phases:

- `on_mesh_begin`: name, UV-layer count, texture count are known; vertex/face counts are not read yet.
- `on_mesh_end`: the final `xps_mesh_info` contains `vertex_count` and `face_count`.

The original `provider_xps89.c` cached only the begin-phase structure and never refreshed it. As a result the runtime advertised the model/mesh and skeleton but visited zero triangles.

The provider now captures stream offsets at `mesh_begin` and refreshes `m->info` from the final structure at `mesh_end`.

Regression coverage includes headered `.xps`, legacy binary `.mesh`, ASCII `.mesh.ascii`, and a boned ASCII fixture.
