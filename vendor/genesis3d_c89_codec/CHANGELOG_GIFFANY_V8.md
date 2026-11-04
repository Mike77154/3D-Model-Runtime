# Giffany v8 — Overdrive / CF00 + ASCII writer pass

Este empuje añade compatibilidad práctica con más salidas reales del toolchain Genesis3D original.

## Cambios grandes

### 1. Writer `.mot` ASCII `MOTN`

v7 ya podía leer `.mot` ASCII. v8 también puede escribirlos:

```c
int g3d_motion_measure_ascii(const g3d_motion *motion, g3d_u32 *size_out);
int g3d_motion_write_ascii(const g3d_motion *motion, void *dst, g3d_u32 cap, g3d_u32 *used_bytes);
```

El writer emite layout original-style:

```text
MOTN
 0.F0
NameID <name>
MaintainNames <0|1>
PathCount <n>
NameChecksum <checksum>
Events <0|1>
[TKEV]
NameArray <0|1>
[SBLK]
PathArray <n>
[PATH 0.F2]
```

`g3dtool` agrega:

```sh
./g3dtool motascii input.mot output_ascii.mot
```

### 2. Parser `PATH 0.F0` legacy

Se agregó el parser del layout viejo del source original:

```text
PATH 0.F0
Looped <0|1>
Rotation <0|1>
CHNL 0.F0
InterpolationType <n>
StartTime <t>
EndTime <t>
KeyList <n>
[QKFL/QKFS/QKFQ keys]
Translation <0|1>
CHNL 0.F0
[VKFL/VKFH keys]
```

Soporta:

- `CHNL 0.F0`
- `QKFL`, `QKFS`, `QKFQ`
- `VKFL`, `VKFH`
- mapeo de interpoladores viejos a los enums públicos del kit

Nota práctica: las tangentes de `VKFH` y el segundo quaternion/corner de `QKFQ` se leen para avanzar el stream, pero la representación pública conserva las claves base y deja que el sampler use la heurística fija del kit.

### 3. VFS `CF00` con máscara Genesis3D

El VFS original usa `VF00` para contenedores normales y `CF00` cuando los payloads de archivos hijos van XOR-eados con una máscara de un byte derivada de un contexto externo.

APIs nuevas:

```c
g3d_u8 g3d_vfs_context_mask_from_string(const char *context);
int g3d_vfs_parse_with_mask(g3d_vfs *vfs, void *bytes, g3d_u32 size, g3d_arena *arena, g3d_u8 xor_mask);

int g3d_body_parse_file_with_mask(g3d_body *body, void *data, g3d_u32 size, g3d_arena *arena, g3d_u8 xor_mask);
int g3d_actor_parse_file_with_mask(g3d_actor *actor, void *data, g3d_u32 size, g3d_arena *arena, g3d_u8 xor_mask);

int g3d_body_write_file_with_mask(const g3d_body *body, void *dst, g3d_u32 cap, g3d_u32 *used_bytes, g3d_u8 xor_mask);
int g3d_actor_write_file_with_mask(const g3d_actor *actor, void *dst, g3d_u32 cap, g3d_u32 *used_bytes, g3d_u8 xor_mask);
```

`g3dtool` agrega:

```sh
./g3dtool makecf  Genesis3 input.act output_cf.act
./g3dtool verifycf Genesis3 output_cf.act
./g3dtool canoncf Genesis3 output_cf.act output_plain.act
```

También acepta máscaras explícitas:

```sh
./g3dtool verifycf 0x67 output_cf.act
```

### 4. Regresiones nuevas

`selftest` ahora cubre:

- cálculo de máscara de contexto estilo `fsvfs.c`
- escritura/lectura `CF00` para `.bdy`
- escritura/lectura `CF00` para `.act`
- rechazo esperado de `CF00` si se usa el parser normal sin máscara
- parser `PATH 0.F0` legacy con `QKFS` + `VKFH`
- writer `.mot` ASCII y round-trip de vuelta al parser

`smoke.sh` ahora cubre:

- `makecf` / `verifycf` / `canoncf`
- rechazo de `verify` normal sobre `CF00`
- round-trip `MOT ASCII -> binario -> ASCII`
- `PATH 0.F0 -> canon -> verify`

## Límites todavía vivos

- `MOTION_NODE_BRANCH` sigue fuera: el branch original también estaba incompleto para serialización completa.
- `CF00` necesita máscara/contexto externo; el archivo no guarda esa máscara.
- `QKFQ` y `VKFH` legacy se parsean de forma compatible, pero no preservan todos los datos auxiliares del canal viejo como representación pública independiente.
- `SQUAD` runtime sigue aproximado por blend normalizado fixed-point.
