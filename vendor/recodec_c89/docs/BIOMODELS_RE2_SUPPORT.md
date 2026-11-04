# BioModels -> recodec RE2 support notes

Este paquete no copia código de BioModels; se usó como referencia de comportamiento para alinear el parser de Resident Evil 2 clásico.

## Soporte agregado

- `MD1` RE2 clásico se lee como BioModels: un descriptor de 56 bytes por limb real.
- `EMD` RE2 usa la sección 7 como malla MD1.
- `PLD` / `PLW` RE2 usan sección 2 como malla MD1 y sección 3 como TIM interno si existe.
- `EMR` se inspecciona para llenar `parent_index` y `pivot` en piezas GFM.
- `TIM` externo de `EMD` se busca como sidecar: `.TIM`, `.tim`, `.Tim`.
- Los `clutId` de caras texturizadas quedan como materiales `re2_clut_N`.

## Lo que todavía queda experimental

- No se evalúan clips `EDD` ni keyframes completos para animación runtime.
- No se recompone una escena skinned final; se conserva la malla base por limbs.
- La reinserción retail perfecta requiere validar límites por personaje, textura/CLUT y empaquetado de ISO/PC.

## Pruebas realizadas

```sh
make
sh tests/smoke.sh
./build/grc convert build/cube.gfm build/cube.md1 --psx
./build/grc info build/cube.md1
./build/grc convert build/cube.gfm build/cube_re2.emd --game re2 --psx
./build/grc info build/cube_re2.emd
./build/grc convert build/cube.gfm build/cube.pld --game re3 --psx
./build/grc info build/cube.pld
```

Resultado local: build correcto y smoke test completo en C89.
