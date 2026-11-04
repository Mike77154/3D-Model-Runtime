# API notes

## Flujo recomendado

```c
xps_probe_result probe;
int rc = xps_probe_file(path, &probe);
if (rc != XPS_OK) { /* ... */ }

switch (probe.format) {
case XPS_FMT_POSE:
    rc = xps_pose_parse_file(path, &pose_sink, user);
    break;
case XPS_FMT_BONEDICT:
    rc = xps_bonedict_parse_file(path, &bonedict_sink, user);
    break;
case XPS_FMT_SCENE:
    rc = xps_scene_parse_file(path, &scene_sink, user);
    break;
case XPS_FMT_XPS:
    rc = xps_header_parse_file(path, &header_sink, user);
    if (rc == XPS_OK) {
        rc = xps_parse_file(path, &model_sink, user);
    }
    break;
default:
    rc = xps_parse_file(path, &model_sink, user);
    break;
}
```


## Header `.xps`

Callbacks disponibles en `xps_header_sink`:

- `on_header_begin`
- `on_header_option`
- `on_header_flag_pair`
- `on_header_flags`
- `on_header_default_pose_begin`
- `on_header_default_pose_bone`
- `on_header_default_pose_end`
- `on_header_end`

Cada `xps_header_info` trae, entre otras cosas:

- versión `version_major/version_minor`
- string `xna_aral`
- `settings_len_words/settings_len_bytes`
- `machine/user/files`
- `settings_kind` (`none`, `legacy_raw`, `typed`)
- conteos de default pose y flags

Notas:

- `xps_header_option` expone `type_raw`, `count`, `info` y `payload_bytes`.
- `xps_header_flag_pair` entrega el par crudo `flag_id/raw_value`.
- `xps_header_flags` entrega el resumen normalizado y también `gloss_raw`.
- la default pose del header reutiliza `xps_pose_bone` para no duplicar layout.

## Modelo

Callbacks disponibles en `xps_sink`:

- `on_model_begin`
- `on_bone`
- `on_mesh_begin`
- `on_texture`
- `on_vertex`
- `on_face`
- `on_mesh_end`
- `on_model_end`

## Pose

Callbacks disponibles en `xps_pose_sink`:

- `on_pose_begin`
- `on_pose_bone`
- `on_pose_end`

Cada `xps_pose_bone` trae:

- nombre del hueso
- rotación delta `rx/ry/rz`
- traslación delta `tx/ty/tz`
- escala `sx/sy/sz`
- bandera `has_explicit_scale`

## BoneDict

Callbacks disponibles en `xps_bonedict_sink`:

- `on_bonedict_begin`
- `on_bonedict_entry`
- `on_bonedict_end`

Cada entrada `xps_bonedict_entry` trae:

- `old_name`
- `new_name`
- índice de aparición

## Scene

Callbacks disponibles en `xps_scene_sink`:

- `on_scene_begin`
- `on_scene_item_begin`
- `on_scene_item_bone`
- `on_scene_accessory`
- `on_scene_item_end`
- `on_scene_camera`
- `on_scene_light`
- `on_scene_post`
- `on_scene_background`
- `on_scene_sky`
- `on_scene_window`
- `on_scene_end`

Notas:

- `on_scene_item_begin` ya conoce `bone_count`, pero la posición final del item queda cerrada recién en `on_scene_item_end`.
- `on_scene_accessory` diferencia primarios y secundarios con `enum xps_scene_accessory_kind`.
- `on_scene_post`, `on_scene_sky` y `on_scene_window` solo se emiten si el bloque existe en esa versión.

## Scan

- `xps_scan_file()` es solo para **modelos**.
- `xps_pose_scan_file()` es solo para **poses**.
- `xps_bonedict_scan_file()` cuenta entradas, `identity_count` y `rename_count`.
- `xps_scene_scan_file()` cuenta items visibles/ocultos, bones de pose y accesorios.
