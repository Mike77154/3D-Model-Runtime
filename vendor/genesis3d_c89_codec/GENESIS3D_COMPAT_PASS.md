# Genesis3D original compatibility pass — Giffany v6

Este pase compara el codec C89 contra las fuentes originales de Genesis3D y aplica compatibilidad binaria donde el kit se había quedado “casi, pero no todavía”.


## Pase adicional v6

Este build agrega otro empujón encima de v5:

- `g3d_vfs_find()` ahora imita mejor `DirTree_FindExact`: comparación ASCII case-insensitive y separadores `/` + `\\`.
- `VKFrame` y `QKFrame` con `LINEARTIME` usan `start_time + index * delta_time`, como el source original.
- `QKFrame` con `HINGE_COMPRESSION` ya se puede samplear en fixed-point: eje + ángulo -> quaternion Q15.
- `g3d_strblock_checksum()` replica el checksum usado por Motion/StrBlock; los MOT escritos auto-rellenan `NameChecksum` si estaba en cero.
- Los writers validan estructuras antes de serializar para evitar ACT/BDY/MOT corruptos por accidente.

## Cambios aplicados

### 1. `SBKB` / `geStrBlock`

**Original revisado:** `Genesis3D-master/Actor/strblock.c`

Genesis3D guarda:

```text
'SBKB'
Count
Size
Data
```

El kit anterior escribía/esperaba `Size,Count`. Ahora:

- lee `Count,Size` como formato oficial;
- valida offsets, orden ascendente y terminadores NUL;
- acepta `Size,Count` como fallback legacy si el formato oficial no valida;
- escribe siempre `Count,Size`.

### 2. `QKFrame` / quaternions

**Original revisado:** `Genesis3D-master/Actor/QKFrame.c` y layout `geQuaternion`.

Genesis3D serializa quaternions en memoria/disco como:

```text
W, X, Y, Z
```

La API del kit conserva `g3d_fquat { x_bits, y_bits, z_bits, w_bits }`, pero el lector/escritor ya traduce:

```text
file WXYZ <-> API XYZW
```

### 3. VFS/DirTree `.act` y `.bdy`

**Original revisado:** `Genesis3D-master/VFile/dirtree.c`, `VFile/fsvfs.c`, `Actor/actor.c`, `Actor/body.c`.

Genesis3D escribe un nodo raíz explícito con nombre vacío `""` y atributo directorio. Antes el kit emitía directamente `Header`/`Geometry` como raíz del árbol. Ahora:

- el writer de `.act` emite raíz vacía + `Header`, `Body`, `Motions`;
- el writer de `.bdy` emite raíz vacía + `Geometry`, `Bitmaps`;
- `g3d_vfs_find()` ignora esa raíz vacía al resolver rutas públicas, manteniendo compatibilidad con árboles legacy sin raíz.

### 4. Tests nuevos

`selftest.c` ahora prueba explícitamente:

- parsing `SBKB` oficial `Count,Size`;
- parsing legacy `SBKB` `Size,Count`;
- escritura/lectura `QKFrame` con file-order `WXYZ`;
- presencia de raíz VFS vacía en ACT escrito por el codec;
- búsqueda pública `Header` / `Motions` sin obligar al usuario a saber de esa raíz interna;
- búsqueda VFS case-insensitive y rutas con `/` o `\\`;
- sampleo de `QKFrame` hinge-compressed;
- `LINEARTIME` como `start + index * delta`;
- auto-fill y validación de `NameChecksum`.

## Verificación realizada

```sh
make clean
make all
make test
make smoke
```

Resultado observado:

```text
OK actor_bytes=1076 body_bytes=481 motion_bytes=150/152 motions=2 vertices=3
smoke OK
```

## Nota sobre Irrlicht

En los zips recibidos no apareció una carpeta `irrlicht-master` ni `irrlitch-master`. El trabajo se aplicó al codec C89 `genesis3d_c89_codec`, que es el módulo ACT/BDY/MOT disponible en el paquete.
