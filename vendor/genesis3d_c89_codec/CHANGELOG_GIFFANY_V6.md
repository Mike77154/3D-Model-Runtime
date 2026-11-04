# Giffany v6 Supersteroids — Genesis3D codec push

Pase adicional encima de v5, comparado contra fuentes originales de Genesis3D para apretar compatibilidad en `.act`, `.bdy` y `.mot`.

## Cambios fuertes

### 1. VFS/DirTree más parecido al original

Original revisado:

```text
Genesis3D-master/VFile/dirtree.c
Genesis3D-master/VFile/dirtree-common.c
Genesis3D-master/VFile/fsvfs.c
```

Genesis3D resuelve nombres con comparación case-insensitive (`stricmp`) y usa `\\` como separador interno. El finder público ahora:

- busca entries de forma ASCII case-insensitive;
- acepta `/` y `\\` como separadores;
- sigue ocultando la raíz vacía `""` del DirTree original al usuario;
- mantiene compatibilidad con árboles legacy del kit.

Ejemplos válidos:

```text
Header
header
Motions/0
MOTIONS\\0
```

### 2. QKFrame hinge compression sampleable

Original revisado:

```text
Genesis3D-master/Actor/QKFrame.c
```

Los QKFrame `HINGE_COMPRESSION` guardan un eje global y un ángulo por key. El sampler fixed-point ya no devuelve `unsupported`; ahora decodifica:

```text
axis + angle -> quaternion Q15
```

Luego interpola con blend normalizado fixed-point, igual que el resto de la ruta runtime del kit.

### 3. LINEARTIME corregido como Start + Delta

Original revisado:

```text
Genesis3D-master/Actor/vkframe.c
Genesis3D-master/Actor/QKFrame.c
```

Genesis3D no guarda `start,end` en lineartime; guarda:

```text
start_time
delta_time
```

El codec ahora calcula:

```text
key_time[i] = start_time + i * delta_time
```

para `VKFrame` y `QKFrame`. `selftest` incluye regresión con tres keys para evitar que vuelva el bug viejo.

### 4. NameChecksum estilo Genesis3D

Original revisado:

```text
Genesis3D-master/Actor/motion.c
Genesis3D-master/Actor/strblock.c
```

Se agregó API pública:

```c
g3d_s32 g3d_strblock_checksum(const g3d_strblock *sb);
```

El writer de `.mot` auto-rellena `leaf.name_checksum` cuando viene en cero y hay `name_array`. El validador también rechaza checksums no-cero que no coinciden con el strblock.

### 5. Validaciones antes de escribir

Los writers de `motion`, `body` y `actor` ahora corren validación antes de serializar, para fallar temprano si el caller trae estructuras inconsistentes.

También se agregó rechazo temprano de `motion_count` negativo al parsear ACT.

### 6. Build más estable

`Makefile` ahora compila `g3d_codec.c` una sola vez en `libg3d_codec.a`; `selftest`, `g3dtool` y `mk_sample` linkean contra esa librería. Esto evita recompilar el núcleo tres veces y hace más liviana la verificación.

### 7. Endurecimiento numérico

- `g3d_q15quat_dot()` usa multiplicación Q15 para evitar overflow firmado en la prueba de signo del quaternion.
- El cálculo `start + index * delta` satura a `INT_MIN/INT_MAX`.
- Las flags de compresión se validan explícitamente:
  - QK: `HINGE` + `LINEARTIME`
  - VK: `LINEARTIME`

## Regresiones nuevas en `selftest.c`

- `SBKB` oficial `Count,Size`.
- Fallback legacy `SBKB` `Size,Count`.
- QKFrame file-order `W,X,Y,Z` con API `X,Y,Z,W`.
- ACT con raíz VFS vacía original.
- VFS case-insensitive y `/` + `\\`.
- QK hinge-compressed sampleado a mitad de una rotación Z de 90 grados.
- VK lineartime `start + delta` con tres keys.
- Auto-fill de `NameChecksum` en motion escrita.

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
smoke OK
```
