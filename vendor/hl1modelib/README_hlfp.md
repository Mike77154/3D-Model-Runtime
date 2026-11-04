# HLFP GoldSrc steroids edition

Esta carpeta ahora incluye la rama `hlfp_semantic_reemit` + el módulo nuevo `hlfp_goldsrc.h/.c`.

Lee primero: `README_GOLDSRC_STEROIDS.md`.

---

# HLFP semantic re-emit + quaternion toolkit

Toolkit C89 para GoldSrc/HL1 con estas piezas activas:

- parser SMD
- parser MDL
- compilador subset SMD -> MDL
- roundtrip binario explícito (`hlfp_mdl_roundtrip_*`)
- cache binaria desacoplada del buffer original
- **re-emisión binaria más semántica** para `main`, `T.mdl` e `IDSQ`
- blending temporal entre frames
- multipose con quaterniones
- controllers para secuencias y bone controllers aditivos

## Lo nuevo en esta rama

### 1) Re-emisión más semántica
Se agregaron:

- `hlfp_mdl_regenerate_main_binary_semantic(...)`
- `hlfp_mdl_regenerate_texture_binary_semantic(...)`
- `hlfp_mdl_regenerate_seqgroup_binary_semantic(...)`

Y las wrappers viejas ahora llaman a la ruta semántica:

- `hlfp_mdl_regenerate_main_binary(...)`
- `hlfp_mdl_regenerate_texture_binary(...)`
- `hlfp_mdl_regenerate_seqgroup_binary(...)`

La regeneración nueva ya no hace copy directo del archivo entero. En cambio:

- reescribe `studiohdr_t` / `studioseqhdr_t`
- reserializa huesos, bone controllers, hitboxes, seqgroups, seqdesc, attachments, texturas, skins, bodyparts, models y meshes
- reconstruye vértices / normales desde el modelo parseado
- re-emite los `tri cmds` en una forma canónica (triángulos explícitos de 3 vértices)
- re-encodea la animación de cada secuencia/blend decodificando poses y cuantizando otra vez con `bone.value[]` + `bone.scale[]`
- reconstruye `T.mdl` e `IDSQ` como binarios válidos mínimos

## Qué sigue siendo opaco / subset
No todo el formato quedó reconstruido desde semántica pura todavía. En esta rama:

- `automoveposindex` y `automoveangleindex` se arrastran como campos opacos
- sound tables / sound groups se emiten en `0`
- los `tri cmds` no preservan strip/fan exactos del original: se canonizan a triángulos explícitos
- los pivots se vuelven a emitir leyendo sus registros desde el `main_buffer` parseado

## Quaterniones + controllers
Se mantienen activas:

- `hlfp_mdl_controller_state_init(...)`
- `hlfp_mdl_pose_to_quat_pose_set(...)`
- `hlfp_mdl_quat_pose_to_local_matrices(...)`
- `hlfp_mdl_quat_pose_to_world_matrices(...)`
- `hlfp_mdl_blend_quat_pose_sets(...)`
- `hlfp_mdl_decode_sequence_frame_fixed_quat_ctrl(...)`
- `hlfp_mdl_decode_sequence_time_quat_ctrl(...)`
- `hlfp_mdl_blend_sequence_times_quat_ctrl(...)`
- `hlfp_mdl_blend_sequence_world_matrices_times_quat_ctrl(...)`

## Build

```sh
make
./test_hlfp
```

## Smoke test actual

El smoke test:

- compila un MDL mínimo desde SMD
- lo parsea con cache binaria propia
- lo re-emite semánticamente
- vuelve a parsear el binario regenerado
- compara estructura base, un vértice y una pose
- prueba blend temporal/quaternion y bone controller aditivo sobre el binario reemitido

Ver salida en `test_output.txt`.

## StudioHalfLife Ultra layer

This package also includes the `StudioHalfLife Ultra` layer:

- `hlfp_studiohalflife_ultra.h/.c`
- `hlfp_softmdl_ultra.h/.c`
- `test_studiohalflife_ultra.c`
- `demo_render_studiohalflife_ultra.c`

See `README_STUDIOHALFLIFE_ULTRA.md` for the body/skin resolver, attachment world transforms, animation driver, IK-rule pass, deep audit, QC supplemental writer, companion manifest, and debug renderer overlay.
