# Model upgrade notes v0.2

## Mejoras reales metidas

- `grc_piece` para conservar piezas/limbs/grupos.
- GFM2 con `piece_count` y tabla de piezas.
- OBJ loader con `mtllib`, `usemtl`, `o/g` y triangulación fan.
- OBJ writer con sidecar `.mtl` y grupos `g` por pieza.
- Validación de índice, degenerados y vértices sin uso.
- Limpieza y preparación PSX centralizadas en `grc_model_prepare()`.
- MD2 writer ajustado al layout que el parser espera para evitar índices degenerados.
- `grc sections` para inspección de contenedores EMD/PLD/PLW.
- Viewer con grid, color por cara y proyección `xy`, `xz`, `yz`.

## Lo que sigue como fase 0.3

```text
1. Resolver armatures/joints reales de EMD/PLD.
2. Animaciones: leer/escribir frame data y exportar a glTF.
3. TIM encode completo y asociación material->TIM/CLUT.
4. Native repack por variante exacta RE1/RE2/RE3.
5. Config por juego/build: PS1 retail, PC, Sourcenext, prototipos.
```
