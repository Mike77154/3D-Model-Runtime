# Giffany v10 - HyperPush VFS + Lookups + Hard Validation

Este empuje toma la v9 como base y añade herramientas y APIs para trabajar mejor con archivos reales de Genesis3D/Reality Factory: inspección de VFS, extracción puntual de payloads, lookups semánticos de bones/motions y validación más estricta.

## 1. APIs VFS nuevas

```c
g3d_u32 g3d_vfs_count_entries(const g3d_vfs *vfs, int include_directories);
g3d_u32 g3d_vfs_count_files(const g3d_vfs *vfs);
```

Sirven para tooling y validación de contenedores `VF00`/`CF00` sin caminar el árbol a mano desde el caller.

## 2. Nuevos comandos VFS en `g3dtool`

```sh
./g3dtool vfsls player.act
./g3dtool vfsget player.act Header header.bin
./g3dtool vfsget player.bdy Geometry geometry.body

./g3dtool vfsls player_cf.act Genesis3
./g3dtool vfsget player_cf.act Header header.bin Genesis3
```

`vfsls` imprime firma, versión, cifrado/máscara, offsets, conteo de entradas y árbol.  `vfsget` extrae un payload exacto por ruta usando el mismo finder case-insensitive que el runtime.

## 3. Lookups semánticos nuevos

```c
const char *g3d_motion_get_path_name(const g3d_motion *motion, g3d_s32 path_index);
g3d_s32 g3d_motion_name_checksum(const g3d_motion *motion);

int g3d_body_find_bone_index(const g3d_body *body, const char *bone_name, g3d_s32 *out_index);
const char *g3d_body_get_bone_name(const g3d_body *body, g3d_s32 bone_index);
g3d_s32 g3d_body_bone_name_checksum(const g3d_body *body);
int g3d_body_compute_bbox_fix16(const g3d_body *body, g3d_fixvec3 *min_out, g3d_fixvec3 *max_out);

int g3d_actor_find_motion_index(const g3d_actor *actor, const char *motion_name, g3d_s32 *out_index);
const char *g3d_actor_get_motion_name(const g3d_actor *actor, g3d_s32 motion_index);
```

Estas APIs evitan duplicar lógica de strblocks y nombres en integraciones con motores o importadores.

## 4. Nuevos comandos de inspección

```sh
./g3dtool bones player.act
./g3dtool bones geometry.body
./g3dtool motions player.act
./g3dtool motions walk.mot
./g3dtool checksum player.act
```

- `bones`: jerarquía, parent index/nombre, bbox por bone, translation del attachment y bbox computada desde vértices.
- `motions`: motions, paths, interpoladores, flags de canales, compresión y checksums.
- `checksum`: checksums de nombres de bones/materiales y path arrays de motions.

## 5. Validación endurecida

- `QKFrame` y `VKFrame` con tiempos explícitos ahora exigen keys en orden estrictamente creciente.
- `LINEARTIME` exige `delta_time > 0` cuando hay más de una key.
- `TKEV/TKEB` valida que offsets estén dentro de `event_data`, que cada evento tenga string NUL-terminated y que los tiempos estén ordenados.
- `g3d_body_validate()` exige jerarquía topológica: el `parent_bone_index` debe apuntar a un bone anterior al hijo.

Esto se alinea mejor con cómo el runtime transforma poses: los padres tienen que existir antes de componer matrices de hijos.

## 6. Regresiones nuevas

`selftest.c` ahora cubre:

- contadores VFS `g3d_vfs_count_entries/files`
- helpers de bone/motion/actor lookup
- bbox computada fixed16 desde vértices
- rechazo de keys fuera de orden
- rechazo de `LINEARTIME` con delta cero

`smoke.sh` ahora cubre:

- `vfsls` sobre `.act` y `.bdy`
- `vfsget Header` y `vfsget Geometry`
- `vfsls/vfsget` sobre `CF00` con contexto `Genesis3`
- `bones`, `motions` y `checksum`
- comparación de payload `Geometry` extraído por VFS vs `extract`

## 7. Compatibilidad conservada

La v10 conserva lo agregado en v5-v9:

- VFS raíz vacía estilo Genesis3D
- `SBKB` oficial `Count,Size` con fallback legacy
- quaternions `W,X,Y,Z` en disco
- ASCII `MOTN` reader/writer
- `PATH 0.F0/F1/F2`
- `CF00` con máscara/contexto
- raw `Geometry/BODY`
- eventos runtime `[start,end)`
- pose blending y skinning fixed-point
