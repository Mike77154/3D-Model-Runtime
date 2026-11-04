# mmd89_anim_codec

Codec/loader C89 para animaciones y poses de MikuMikuDance:

- `.vmd` **Vocaloid Motion Data**: lector y escritor binario.
- `.vpd` **Vocaloid Pose Data**: lector y escritor textual, más conversión VPD → VMD de un solo frame.
- `.pmm` **Polygon Movie Maker project**: scanner/inspector de proyecto para localizar rutas de assets embebidas; no intenta reconstruir todo el proyecto porque PMM está poco documentado y depende de assets externos.

## Contrato técnico

- C89.
- Sin `malloc`, `free`, `realloc`, ni buffers de heap dentro de la librería.
- Sin tipos `float` ni `double`.
- Todo valor animable queda en **Q16.16 fixed point** (`mmd89_q16`).
- Las entradas/salidas son buffers del usuario.
- Los límites se controlan por macros `MMD89_MAX_*` antes de incluir `mmd89_anim.h`.

## Qué soporta VMD

Lee/escribe estas secciones:

- header + model name.
- bone keyframes: nombre, frame, posición, rotación quaternion, 64 bytes de interpolación.
- morph/facial keyframes: nombre, frame, weight.
- camera keyframes: frame, distance, target, rotation, 24 bytes de interpolación, FOV, perspective.
- light keyframes.
- shadow keyframes.
- IK/display frames con entradas variables.

La conversión de los valores IEEE-754 almacenados por VMD se hace a Q16.16 mediante manipulación entera de bits, sin usar tipos flotantes.

## Qué soporta VPD

El parser acepta el patrón típico:

```text
Vocaloid Pose Data file

model.osm;

N;

Bone0{bone_name
  x,y,z;
  qx,qy,qz,qw;
}
```

Guarda los nombres como bytes crudos para no romper Shift-JIS/Japonés.

## PMM: alcance honesto

PMM es el archivo de proyecto de MMD/Polygon Movie Maker. El scanner incluido busca rutas visibles de modelos, motions, poses, accesorios, audio, imágenes y efectos en ASCII o UTF-16LE básico:

- `.pmx`, `.pmd`
- `.vmd`, `.vpd`
- `.x`
- `.wav`
- `.bmp`, `.jpg`, `.jpeg`, `.png`, `.tga`, `.sph`, `.spa`
- `.fx`, `.emm`
- `.pmm`

Devuelve `MMD89_WARN_PMM_PARTIAL` porque no pretende ser un loader completo de escena PMM.

## Compilar

```sh
make
make check
```

Usa:

```sh
gcc -std=c89 -O2 -Wall -Wextra -pedantic -Iinclude
```

## CLI

```sh
./mmd89_dump motion.vmd
./mmd89_dump pose.vpd
./mmd89_dump project.pmm
```

## API mínima

```c
static mmd89_motion_t motion;
static mmd89_pose_t pose;
static unsigned char out[1024 * 1024];
static mmd89_u32 out_size;

mmd89_vmd_read_memory(data, size, &motion);
mmd89_vpd_read_memory(data, size, &pose);
mmd89_vpd_to_vmd_frame0(&pose, "model", &motion);
mmd89_vmd_write_memory(&motion, out, sizeof(out), &out_size);
```

## Archivos

```text
include/mmd89_anim.h      API pública
src/mmd89_anim.c          implementación
examples/example_memory.c ejemplo VPD→VMD sin archivos
tests/selftest.c          prueba de roundtrip
 tools/mmd89_dump.c       inspector CLI
Makefile                  build C89
```

## Notas de integración con engine

- Para tu runner, trata `mmd89_motion_t` como IR estático de animación.
- Los nombres de huesos/morphs vienen en bytes crudos; el engine debe comparar contra nombres del PMX/PMD en el mismo encoding o tener una tabla puente.
- El codec no aplica skinning ni resuelve IK; sólo importa/exporta claves y poses.
- La interpolación Bezier queda preservada como bytes; la evaluación de curvas puede vivir en tu runtime de animación fixed-point.
