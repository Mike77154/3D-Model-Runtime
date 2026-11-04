# Fase 10 — Máscaras de huesos y root motion (3DGS A4–A8)

## Contrato

- **C89 estricto**, `long`/`unsigned long`, Q16.16, no `malloc`/`calloc`/`realloc`/`free`, no `float`/`double`/`long long` en los módulos nuevos.
- `GS3D_BoneMask`: `mask_init(mask, 0)` deja todo el esqueleto en la animación base; `mask_subtree(mask, spine_index, 65536)` incluye un hueso y sus descendientes.
- La máscara no describe un campo binario MDL: es una **función de reproducción del motor**.
- `gs3d_layer_apply` mezcla las transformaciones **locales** de los huesos y reconstruye su jerarquía antes de deformar vértices.
- La contribución final para un hueso es `overlay_weight_q16 * mask[bone]` (Q16.16).
- El vertex-morph de MDL7, si existe, proviene exclusivamente del **clip base**; la mezcla parcial de morphs por región geométrica no está implementada. No se mezclan mallas ya deformadas.
- `gs3d_layer_apply_all` valida rangos de todos los grupos con fotogramas antes de aplicarlos. Los grupos sin fotogramas se dejan como estaban.

## Root motion

- `gs3d_root_motion_delta` entrega **posición local y desplazamiento XYZ** de un hueso raíz real (`parent == 65535`) en Q16.16.
- Sólo `CLAMP` con tiempos no decrecientes; rechaza `LOOP` y `PINGPONG` hasta implementar desenvuelto correcto de ciclos.
- `consume_root_motion=1` fuerza a cero la traslación local del hueso raíz en la pose visual, y **requiere** `root_out`. El movimiento del actor en el mundo lo debe aplicar la aplicación usando `root_out.delta_q16`.
- Para consumir root motion, la máscara de la raíz ha de valer 0; así una capa de gesto no sustituye accidentalmente la locomoción.
- No extrae orientación/yaw de raíz, contactos de pie, retargeting ni desplazamiento global en un escenario. Son mejoras futuras.

## Rutas por vendor

| Vendor | Familias | `gs3d_vendor_layer_apply()` |
| --- | --- | --- |
| A4 | MDL3 | No compatible: sólo morph por vértices |
| A5 | MDL4/MDL5 | No compatible: sólo morph por vértices |
| A6 | MDL4/5/7 | Compatible con MDL7 esquelético |
| A7 | MDL4/5/7 | Compatible con MDL7 esquelético |
| A8 | MDL4/5/7 | Compatible con MDL7 esquelético |

## Ejemplo de API

```c
GS3D_BoneMask mask;
GS3D_LayerRequest req;
GS3D_RootDelta root;
/* Las entradas de clips GS3D_Clip ya se han creado con gs3d_clip_create. */
if (gs3d_mask_init(&mask,0L) == GS3D_OK) {
    gs3d_mask_subtree(&mask,1U,65536L); /* seleccionar rama del brazo */
    /* req.base / req.overlay y demás campos deben configurarse. */
    gs3d_vendor_layer_apply(7U,&req,&mask,&root);
}
```

## Compilación y test

```bash
sh scripts/check_phase10.sh
```

Prueba de OBJ en fixture sintético original (hueso 1 en capa):

```bash
./gs3d_layer tests/fixtures/mdl7_skeletal_sequence.mdl demo_layer 7 0 0 2 1 1 50 0 100 1 65536 1
```

El instrumento CLI devuelve un delta de raíz `(131072,0,0)` en Q16.16, es decir, X=2 unidades; genera `demo_layer.obj` y `demo_layer.mtl`.

## Pruebas / límites

- Pruebas `test_phase10.c` con máscaras 0%, 50%, 100%, subárbol, diferencias de traslación y retorno al origen; control de overflow de pesos y ciclos parentales.
- `test_phase10_robust.c`: 661 truncamientos + 900 mutaciones de un bit sobre fixture de 660 bytes, compiladas con ASan y UBSan en Linux.
- Se conserva el lector de los cinco vendors y los formatos legacy.
- **No validados** los pesos/deformadores con un modelo MDL7 comercial animado, ni la compilación real MinGW32 Windows.
- El codec continúa **independiente**, no reemplaza el Runtime 3D principal.

Referencias (conceptuales, no requeridas para compilar):
- https://dev.epicgames.com/documentation/unreal-engine/blend-masks-and-blend-profiles-in-unreal-engine
- https://dev.epicgames.com/documentation/unreal-engine/animation-blueprint-blend-nodes-in-unreal-engine
