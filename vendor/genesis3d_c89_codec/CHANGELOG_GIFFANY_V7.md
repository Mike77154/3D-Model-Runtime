# Giffany v7 ASCII MOT — Genesis3D codec push

Pase adicional encima de v6, apoyado directamente en el source original de Genesis3D y en sus tools. El foco de este empuje fue cerrar una brecha importante: los `.mot` sueltos creados por herramientas originales como `mkmotion`, `mkactor/AStudio`, Fusion y otras rutas de tooling se escriben como **ASCII `MOTN`**, mientras que los motions embebidos dentro de `.act` suelen ir como **binario `MTNB`**.

## Cambios fuertes

### 1. Parser de `.mot` ASCII `MOTN`

Original revisado:

```text
Genesis3D-master/Actor/motion.c
Genesis3D-Tools-master/mkactor/mkmotion/mkmotion.c
Genesis3D-Tools-master/mkactor/AStudio/make.c
Genesis3D-Tools-master/Fusion2/model.c
Genesis3D-Tools-master/GBSPLib/Bsp2.cpp
```

El core ahora acepta motions standalone generadas por `geMotion_WriteToFile()`:

```text
MOTN
 0.F0
NameID ...
MaintainNames ...
PathCount ...
NameChecksum ...
Events ...
NameArray ...
PathArray ...
```

`g3d_motion_parse_binary()` conserva el nombre público por compatibilidad, pero ahora autodetecta:

```text
MTNB -> motion binaria
MOTN -> motion ASCII original Genesis3D
```

### 2. Parser ASCII `PATH` F1/F2

Original revisado:

```text
Genesis3D-master/Actor/path.c
```

Se agregó lectura de paths ASCII con:

```text
PATH
 0.F2
Rotation <flag> <path-interp>
Keys ...
Translation <flag> <path-interp>
Keys ...
```

Soporta `PATH` ASCII `0.F1` y `0.F2`. La variante `0.F0` antigua queda rechazada como versión no soportada porque usa una estructura de canales vieja distinta a la ruta moderna de `Rotation/Translation`.

### 3. Parser ASCII `QKFrame` y `VKFrame`

Original revisado:

```text
Genesis3D-master/Actor/QKFrame.c
Genesis3D-master/Actor/vkframe.c
```

Se agregó parseo de:

- `Keys <count> <interpolation> <compression> <looping>`
- `LINEARTIME` como `start_time, delta_time`
- quaternions ASCII en orden de archivo `W, X, Y, Z`
- `QKFRAME_HINGE_COMPRESSION` como eje + ángulos
- comentarios de writer original al final de línea, por ejemplo:

```text
0.000000 0.500000 Start T,Delta T
0.000000 0.000000 1.000000 Axis
```

La API pública conserva `g3d_fquat { x, y, z, w }`; el codec traduce internamente.

### 4. Parser ASCII `SBLK` / `geStrBlock`

Original revisado:

```text
Genesis3D-master/Actor/strblock.c
```

Además de `SBKB` binario, ahora se lee:

```text
SBLK
 0.F0
Strings N
<string-0>
<string-1>
...
```

El parser arma la misma representación interna que el binario: tabla de offsets `s32[count]` + strings NUL-terminated.

### 5. Parser ASCII `TKEV` / `geTKEvents`

Original revisado:

```text
Genesis3D-master/Actor/tkevents.c
```

Se agregó lectura de eventos ASCII:

```text
TKEV
 0.F0
DataSize N
TimeKeys K
<time> <offset>
<event string>
```

Esto permite que `g3dtool info`, `dump`, `verify`, `pose` y `canon` funcionen con `.mot` sueltos que traen eventos.

### 6. CLI y smoke ahora prueban motions ASCII originales

`smoke.sh` ahora genera un `.mot` ASCII estilo Genesis3D original, lo verifica, lo samplea y lo canonicaliza a binario:

```sh
./g3dtool verify _smoke/original_ascii.mot
./g3dtool pose _smoke/original_ascii.mot 0.5
./g3dtool canon _smoke/original_ascii.mot _smoke/original_ascii_canon.mot
./g3dtool verify _smoke/original_ascii_canon.mot
```

## Regresiones nuevas en `selftest.c`

- `.mot` ASCII `MOTN` completo con:
  - `NameID`
  - `Events 1`
  - `TKEV` ASCII
  - `SBLK` ASCII
  - `PATH` ASCII F2
  - `QKFrame` y `VKFrame` sin compresión
- `.mot` ASCII con:
  - `QKFrame` hinge + lineartime
  - `VKFrame` lineartime
  - comentarios finales del writer original (`Start T,Delta T`, `Axis`)
- sampling fixed-point sobre ambos motions ASCII.
- canonicalización CLI de ASCII MOT a binario MOT.

## Límites honestos

- `MOTION_NODE_BRANCH` sigue como no soportado, igual que el writer/reader branch original estaba marcado como unfinished.
- `PATH` ASCII `0.F0` queda rechazado porque usa el formato viejo de canales, distinto del layout moderno F1/F2.
- `CF00` VFS masked/xor sigue rechazado; `VF00` es la ruta normal soportada.

## Verificación

```sh
make clean
make all
make test
make smoke
```

Resultado observado:

```text
OK actor_bytes=1076 body_bytes=481 motion_bytes=150/152 motions=2 vertices=3
OK type=MOT file=_smoke/original_ascii.mot bytes=407
pose motion=asciiwalk time=0.5000 extents=[0.0000,1.0000] paths=1
  path[0] name=Root t=(5.0000,0.0000,0.0000) q=(0.0000,0.0000,0.0000,1.0000)
wrote _smoke/original_ascii_canon.mot (188 bytes) type=MOT
smoke OK
```
