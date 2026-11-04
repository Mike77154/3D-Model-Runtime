# tests

Fixtures mínimas para validar la librería:

- `sample_ascii.mesh.ascii`
- `sample_ascii_bones.mesh.ascii`
- `sample_legacy.mesh`
- `sample_v3.xps`
- `sample_header_settings.xps`
- `sample_pose.pose`
- `sample_pose_short.pose`
- `sample_bonedict.txt`
- `sample_scene.scene`

La idea es correr `example_dump` sobre cada archivo y verificar que:

- el probe detecta el formato correcto
- los conteos salen consistentes
- las poses leen 9 valores o 6 valores + escala implícita
- BoneDict detecta entradas y mapeos
- Scene recorre items, huesos, accesorios y bloques globales

- `sample_header_settings.xps` cubre el header `.xps` tipado con `flags`, `default pose` y una opción `none`.
