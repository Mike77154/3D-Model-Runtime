# hlfp_goldsrc_steroids

Rama vitaminada de `halflife_md_opener/hlfp_semantic_reemit` para abrir, inspeccionar, evaluar y exportar modelos GoldSrc / Half-Life MDL v10 sin heap.

## Filosofía técnica

- C89.
- Sin `malloc`, `calloc`, `realloc` ni `free`.
- Sin `float` ni `double` como tipos de runtime.
- Coordenadas, UV normalizadas, matrices y poses en fixed-point 16.16.
- El formato MDL guarda campos `float` en disco; la librería los lee/escribe por bits y los convierte a fixed.
- Buffers siempre propiedad del caller.

## Qué trae esta mordida

Se conservó todo lo que ya traía `hlfp_semantic_reemit`:

- Parser SMD.
- Parser MDL v10.
- Compilador subset SMD -> MDL.
- Reemisión semántica de `main`, `T.mdl` e `IDSQ`.
- Decode de animación por frame/tiempo.
- Blending temporal.
- Quaternion poses.
- Bone controllers.

Y se agregó `hlfp_goldsrc.h/.c` con:

1. **Nombres companion GoldSrc**
   - `baseT.mdl` / `baseT.dol`.
   - `base01.mdl`, `base02.mdl`, ... / `.dol`.

2. **Bodygroups estilo engine/HLMV**
   - Extraer elección actual de un body value.
   - Cambiar elección de bodypart.
   - Resolver `model_index` activo por bodygroup.

3. **Skin families reales**
   - Resolver `skinref -> texture_index` por familia.
   - Expandir mesh con textura correcta por skin family.

4. **Texturas GoldSrc paletizadas a RGBA32**
   - Convierte pixels 8-bit + palette 256xRGB a RGBA.
   - Maneja alpha para `HLFP_STUDIO_NF_MASKED` usando índice 255.

5. **Bind pose + bake animado**
   - Construye bind pose desde `bone.value[]`.
   - Transforma vértices/normales con matrices de hueso.
   - Exporta mesh ya skinned/baked a un frame de secuencia.

6. **OBJ writer sin heap**
   - Exporta body activo + skin family + matrices.
   - Exporta una secuencia/frame directo a OBJ.
   - Números escritos como decimales fixed, sin `float`/`double`.

7. **QC scaffold writer**
   - Genera una base QC con `$modelname`, `$body`/`$bodygroup`, `$bbox`, `$cbox`, `$eyeposition`, `$hbox`, `$attachment`, `$controller`, `$sequence`.
   - Ojo: es scaffold útil para decompilar/inspeccionar; no pretende ser byte-perfect StudioMDL todavía.

8. **Reporte diagnóstico**
   - Cuenta bones, bodyparts, models, meshes, textures, skinfamilies, sequences, seqgroups.
   - Detecta padres inválidos, bodypart bases malas, mesh parents malos, skinrefs inválidos, textura externa faltante y seqgroups no adjuntos.

## Build

```sh
make clean all test_output.txt
cat test_output.txt
```

Salida esperada en esta entrega:

```txt
semantic_regen_ok=1 orig_size=6080 regen_size=6080
seqs=2 meshes=1 textures=1
vertex0=(65536,0,0) bone=0
pose_z=196602
quat_blend_xrot_m11=4638 m21=65370 m12=-65370 m22=4638
controller_xrot_m11=46342 m21=46340 m12=-46340 m22=46342
done
goldsrc_steroids_ok=1 mdl_size=5888 body=0 model=0 mesh_v=3 mesh_t=1 tex=0 rgba=64x64/16384 obj=281 qc=380 report=432
goldsrc_export_checks=companion_names bodygroups skins rgba bindpose bakedmesh obj qc report
```

## Uso mínimo

```c
#include "hlfp.h"
#include "hlfp_goldsrc.h"

static hlfp_u8 obj_buffer[65536];
static hlfp_mem_writer writer;
hlfp_mdl mdl;
hlfp_error err;

/* después de hlfp_mdl_parse_main(...) y attach de T/seqgroups si aplica */
hlfp_mem_writer_init(&writer, obj_buffer, sizeof(obj_buffer));
hlfp_goldsrc_export_obj_sequence_frame(&mdl,
                                       0,  /* body value */
                                       0,  /* skin family */
                                       0,  /* sequence */
                                       0,  /* blend */
                                       0,  /* frame */
                                       hlfp_mem_writer_write,
                                       &writer,
                                       &err);
```

## Carga recomendada de un modelo real

La librería no abre archivos por sí sola para no meter I/O ni heap. El caller hace:

1. Leer `modelo.mdl` en un buffer propio.
2. `hlfp_mdl_parse_main(...)` o `hlfp_mdl_parse_main_cached(...)`.
3. Si el main trae `numtextures == 0`, construir nombre con `hlfp_goldsrc_make_texture_filename(...)`, leer `modeloT.mdl` y llamar `hlfp_mdl_attach_texture_file(...)`.
4. Si `numseqgroups > 1`, construir `modelo01.mdl`, `modelo02.mdl`, etc. con `hlfp_goldsrc_make_seqgroup_filename(...)` y llamar `hlfp_mdl_attach_seqgroup_file(...)`.
5. Exportar/diagnosticar/bakear.

## Límites todavía vivos

- No parsea completamente extensiones Xash `studiohdr2`, boneinfo procedural, boneweights extras, IK extendido ni jigglebones avanzados.
- No es un decompilador QC byte-perfect.
- El OBJ exporta geometría horneada, no rig FBX/DAE.
- No resuelve texturas externas tipo reemplazos de mods más allá de `T.mdl`/familias de skin.
- No hay renderer de preview; esta mordida deja los datos listos para tu renderer/engine.
