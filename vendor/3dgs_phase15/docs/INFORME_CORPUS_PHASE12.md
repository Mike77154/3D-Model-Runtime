# Informe de corpus MDL7 — Fase 12

Los modelos auténticos proporcionados fueron abiertos por el códec C89 actualizado y convertidos a OBJ/MTL/TGA. Los modelos originales no están incluidos en este paquete de previsualizaciones.

## Auditoría de los seis modelos

### Goddess4arms
```text
format=MDL7 size=3575890 groups=1 bones=44 verts=2625 triangles=2580
mdl_payload=3415134 ent_lump=0 med_lump=160756 trailing=0
group=0 name= vertices=2625 faces=2580 uvs=2589 skins=1 frames=37 vertex_stride=26 deformers=0/0 max_weights=0 influences=0 weighted_vertices=0
 skin=0 color=1 image=1 diffuse_q16=(52428,52428,52428) opacity=65536
```

### LadyJ
```text
format=MDL7 size=4314070 groups=1 bones=26 verts=2377 triangles=2016
mdl_payload=4314070 ent_lump=0 med_lump=0 trailing=0
group=0 name= vertices=2377 faces=2016 uvs=2377 skins=1 frames=1 vertex_stride=26 deformers=0/0 max_weights=0 influences=0 weighted_vertices=0
 skin=0 color=1 image=1 diffuse_q16=(65536,65536,65536) opacity=65536
```

### NinjaGirlShinobi1
```text
format=MDL7 size=6776746 groups=1 bones=27 verts=410 triangles=816
mdl_payload=6776746 ent_lump=0 med_lump=0 trailing=0
group=0 name= vertices=410 faces=816 uvs=1632 skins=1 frames=120 vertex_stride=26 deformers=0/0 max_weights=0 influences=0 weighted_vertices=0
 skin=0 color=0 image=1 diffuse_q16=(65536,65536,65536) opacity=65536
```

### ShinobiJ
```text
format=MDL7 size=4289106 groups=1 bones=27 verts=1759 triangles=1860
mdl_payload=4289106 ent_lump=0 med_lump=0 trailing=0
group=0 name= vertices=1759 faces=1860 uvs=1759 skins=1 frames=1 vertex_stride=26 deformers=0/0 max_weights=0 influences=0 weighted_vertices=0
 skin=0 color=1 image=1 diffuse_q16=(65536,65536,65536) opacity=65536
```

### Tusker
```text
format=MDL7 size=17586402 groups=1 bones=69 verts=1921 triangles=3732
mdl_payload=17586402 ent_lump=0 med_lump=0 trailing=0
group=0 name= vertices=1921 faces=3732 uvs=2502 skins=4 frames=139 vertex_stride=26 deformers=1/1 max_weights=10 influences=5953 weighted_vertices=1921
 skin=0 color=0 image=1 diffuse_q16=(65536,65536,65536) opacity=65536
 skin=1 color=0 image=1 diffuse_q16=(65536,65536,65536) opacity=65536
 skin=2 color=0 image=1 diffuse_q16=(65536,65536,65536) opacity=65536
 skin=3 color=0 image=1 diffuse_q16=(65536,65536,65536) opacity=65536
```

### TuskerSpear
```text
format=MDL7 size=17373890 groups=1 bones=69 verts=45 triangles=86
mdl_payload=17373890 ent_lump=0 med_lump=0 trailing=0
group=0 name= vertices=45 faces=86 uvs=74 skins=4 frames=139 vertex_stride=26 deformers=0/0 max_weights=0 influences=0 weighted_vertices=0
 skin=0 color=0 image=1 diffuse_q16=(65536,65536,65536) opacity=65536
 skin=1 color=0 image=1 diffuse_q16=(65536,65536,65536) opacity=65536
 skin=2 color=0 image=1 diffuse_q16=(65536,65536,65536) opacity=65536
 skin=3 color=0 image=1 diffuse_q16=(65536,65536,65536) opacity=65536
```

## Limitaciones de la preview

- Rasterización por software usando geometría y UV de los OBJ/TGA exportados por el códec.
- La vista estática no equivale a un render en Acknex: quedan diferencias posibles de iluminación, doble cara y shaders.
- Las poses 0/60, 0/18 y 0/30 son experimentales: no se ha validado la convención de matrices frente al runtime original.
- Los nombres de fotogramas y los registros de pesos reales se exponen sin recrear escenas no presentes en el corpus.

## Ejecución del programa C89

```sh
sh scripts/check_phase12.sh
./gs3d_audit ruta/al/modelo.mdl
./gs3d_export ruta/al/modelo.mdl exportado/modelo
```
