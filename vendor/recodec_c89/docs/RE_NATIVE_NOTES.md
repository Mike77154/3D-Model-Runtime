# Notas Resident Evil 1/2/3

## Mapa de formatos del codec

```text
RE1:
  TMD: item/object/room model PS1-style
  EMD: character/enemy container, experimental
  EMW: weapon model, tratado como familia EMD/weapon en futuras fases

RE2:
  MD1: custom 3D model block
  EMD: enemy model container
  PLD: player model container
  PLW: weapon model container
  TIM: texture image

RE3:
  MD2: custom 3D model block
  EMD: enemy model container
  PLD: player model container
  PLW: weapon model container
  TIM: texture image
```

## TMD

El TMD de PS1 tiene header, tabla de objetos, vertices, normals y primitives. El codec importa objetos y reconstruye caras mediante heurísticas de índices. El exportador escribe un subset flat/untextured útil para roundtrip y tooling.

## MD1 / RE2 mesh style

El importador v0.3 sigue el layout observado en BioModels para Resident Evil 2 clásico:

```text
Md1Header:
  u32 length
  u32 unknown
  u32 meshCountTimesTwo

Por cada limb real:
  Md1MeshHeader de 56 bytes
  triVertexOffset / triVertexCount
  triNormalOffset / triNormalCount
  triIndexOffset / triCount / triTextureOffset
  quadVertexOffset / quadVertexCount
  quadNormalOffset / quadNormalCount
  quadIndexOffset / quadCount / quadTextureOffset
```

`meshCountTimesTwo / 2` da el número de limbs reales. Cada limb conserva una sola pieza en GFM, compartiendo vértices/normales entre triángulos y quads. El codec exporta el mismo estilo de bloque MD1 simple para tooling/roundtrip.

Los UVs se leen desde las tablas de textura MD1. El `clutId` se conserva como material placeholder `re2_clut_N`; cuando hay TIM interno en PLD/PLW o TIM sidecar de EMD, se adjunta como textura RGBA al modelo.

## MD2 / RE3 mesh style

El writer v0.2 corrige el orden de índices para que el roundtrip de triángulos no genere caras degeneradas. RE3 sigue siendo especial: algunos personajes tienen vertex binding/skinning que todavía no se reconstruye.

## EMD / PLD / PLW

El codec puede leer/escribir contenedores simples con sección de malla en índices esperados. `grc sections` ayuda a inspeccionar el layout.

Layout RE2 según BioModels:

```text
PLD/PLW:
  header: beginOffset + offsetCount=4
  offsets: EDD animation, EMR skeleton, MD1 mesh, TIM texture

EMD:
  header: beginOffset + offsetCount=8
  offsets: unknown/reserved, EDD0, EMR0, EDD1, EMR1, EDD2, EMR2, MD1 mesh
  texture: TIM externo con el mismo basename cuando existe
```

La lectura EMR actual no evalúa animación completa; sí usa el árbol de huesos para poblar `piece.parent_index` y `piece.pivot`, suficiente para que un editor/world viewer pueda reconstruir jerarquía base por limbs.

## Límites prácticos

```text
PS1/native:
  vertices por modelo: idealmente <= 65535 para RE2-style, <= 255 por limb en RE3-style export simple
  coords: i16, clamped/quantized con --psx
  quads: soportados en IR, pero triangulate ayuda al tooling moderno
  texturas: TIM decode listo; reinserción textura/material nativa aún parcial
```
