# Arquitectura: native providers, no universal model

## Invariantes

1. El archivo nativo y su codec son la autoridad semántica.
2. `model_runtime89` no crea un `GenericModel` persistente.
3. Los providers se seleccionan por probe y extensión, no por una conversión previa.
4. Las capabilities describen operaciones disponibles; no obligan a que todos los formatos tengan la misma semántica.
5. Los visitors reciben vistas transitorias suficientes para render/inspección.
6. El renderer vive fuera del runtime.
7. La memoria pertenece al caller: arena/workspace, sin heap en el runtime.
8. Las asociaciones entre assets (por ejemplo PMX + VMD) deben ser una capa explícita, no una conversión silenciosa.

## Flujo de open

```text
modelrt_open_file
  -> lee cabecera pequeña
  -> provider.probe(path, head)
  -> el provider elegido parsea a SU estado nativo
  -> handle.provider + handle.native_state
  -> model_info + capabilities
```

## Flujo de render

```text
renderer
  -> modelrt_visit(handle, animation, time, sink)
  -> provider.visit(...)
  -> callbacks material/triangle/bone
  -> renderer consume inmediatamente
```

## Extender con otro formato

Implementar un `modelrt_provider` con:

- `probe`
- `open`
- `close`
- `visit`
- opcional: `animation_count/info`
- opcional: `texture_count/info/decode`

Después registrarlo en `modelrt_register_builtin_providers()` o en un registro del host.

## DirectX `.x` provider

`provider_x89.c` usa `xparsermodget98 v0.5` como parser SAX nativo. Geometría y animación se capturan en pasadas independientes sobre storage caller-owned. Frames, materiales, referencias de textura y SkinWeights conservan nombres/índices de origen. `xp_anim_pose` expone múltiples AnimationSet, source keys, first/last tick y frame_count, y reconstruye poses locales densas bajo demanda; un clip sólo se aplica si todos sus targets resuelven a Frames estables.
