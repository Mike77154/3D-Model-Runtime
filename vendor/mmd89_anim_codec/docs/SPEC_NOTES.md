# SPEC_NOTES

## VMD

Estructura implementada:

```text
30 bytes signature
10 or 20 bytes model name
u32 bone_count
  bone_count * 111 bytes
u32 morph_count
  morph_count * 23 bytes
u32 camera_count
  camera_count * 61 bytes
u32 light_count
  light_count * 28 bytes
u32 shadow_count
  shadow_count * 9 bytes
u32 ik_count
  ik_count * variable records
```

Los archivos viejos pueden cortar antes de camera/light/shadow/IK. En ese caso el loader devuelve OK y marca `MMD89_WARN_EARLY_EOF`.

## VPD

VPD se maneja como pose de un solo frame. La conversión a VMD crea bone keyframes en frame 0.

## PMM

El scanner es deliberadamente conservador. PMM guarda el proyecto MMD; no es un contenedor portable de todos los assets. Por eso aquí se extraen rutas detectables, no animaciones completas.
