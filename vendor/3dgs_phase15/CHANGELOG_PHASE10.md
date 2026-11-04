# 3DGS C89 — Fase 10 (sólo fuentes)

- 11 módulos nuevos: 5 de `layers/`, un router y cinco adaptadores `vendors/a4..a8/`.
- `GS3D_BoneMask`: peso por hueso, selección de descendientes, integridad de la jerarquía y validación de rangos.
- `GS3D_LayerRequest`: base y overlay con relojes propios, factor global Q16, aplicación individual y multigrupo.
- Transformaciones TRS locales de huesos combinadas **antes del único skinning**; reemplazo geométrico por vértices del clip base.
- `GS3D_RootDelta` y `gs3d_root_motion_delta` para root motion CLAMP, consumo opcional de traducción de raíz sin doble desplazamiento.
- Tool `gs3d_layer` genera OBJ/MTL de una pose parcial.
- Nuevos tests: máscaras, raíz, verificación de vendors, ciclos, 661 truncamientos y 900 mutaciones.
- Comandos en `scripts/check_phase10.sh` y actualización de los manifiestos de compilación.
- Sin cambios en el Model Runtime externo. No se afirma validación de pesos en juegos comerciales.
