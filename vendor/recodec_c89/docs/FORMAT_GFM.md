# GFM: Giffy Format Model

GFM es el formato intermedio de la librería. Está diseñado para engine/editor/visor: simple, binario, little-endian, C89-friendly y sin dependencias.

## GFM2 header

```text
char magic[4] = "GFM2"
u32 version = 2
u32 flags
u32 source_game
u32 source_format
u32 vertex_count
u32 normal_count
u32 uv_count
u32 face_count
u32 material_count
u32 texture_count
u32 piece_count
char name[64]
```

GFM1 todavía se lee por compatibilidad, pero `grc_save_gfm()` escribe GFM2.

## Secciones

```text
vertices:  vec3 float32 * vertex_count
normals:   vec3 float32 * normal_count
uvs:       vec2 float32 * uv_count
faces:     fixed 64 bytes * face_count
materials: fixed 72 bytes * material_count
textures:  name[64] + width + height + RGBA8888 bytes
pieces:    fixed 120 bytes * piece_count
```

## Face record

```text
u32 vertex_count       # 3 o 4
i32 v[4]
i32 vt[4]
i32 vn[4]
u8  r,g,b,a
i32 material_index
u32 flags
```

## Piece / limb record

```text
char name[64]
u32 first_vertex
u32 vertex_count
u32 first_normal
u32 normal_count
u32 first_uv
u32 uv_count
u32 first_face
u32 face_count
i32 parent_index
float pivot_x, pivot_y, pivot_z
u32 native_id
u32 flags
```

`piece` representa una parte lógica del modelo: objeto TMD, limb EMD/PLD/MD, o grupo OBJ. Es intencionalmente simple: el codec no promete armature completo todavía, pero ya conserva rangos de caras/vértices para editor y visor.

## Compatibilidad

- `GFM1` se puede leer.
- `GFM2` se escribe por defecto.
- Los campos nuevos se añaden al final para no romper el core vertex/face/material.
