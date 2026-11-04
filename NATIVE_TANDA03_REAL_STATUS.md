# Modelo Runtime Experimental — Tanda real de 10 proveedores (65 registrados)

## Corrección del checkpoint anterior

El checkpoint previo etiquetado **65 Providers** contenía únicamente símbolos/documentos de marcador (`provider_native_batch7a_89.c` y `provider_native_batch7b_89.c`), sin registrarlos ni conectarlos a las API de decodificación. Su contador real seguía siendo **55**. Esta entrega parte del último checkpoint plenamente enlazado (55) y agrega **10 adaptadores que sí llaman a los codecs originales**, llegando a **65 proveedores compilados y registrados**; **no 75**. 

## Reglas técnicas

- C89 estricto, sin heap en los wrappers; memory arena del runtime.
- No enlaza GACKT-LANG, passthrough ni artefactos `.gmesh`/`.gscene`.
- Mantiene los 55 proveedores anteriores y sus IDs.
- Es un checkpoint **experimental**, no certificación de los 10 nuevos.
- IDs nuevos: 81, 83, 84, 85, 86, 87, 88, 89, 90, 91. **82 IQM no está conectado:** código vendor con `long long`, incompatible con estricto ISO C89 sin portar aritmética.
- Los codecs de formato secundario no se declaran completamente implementados: Panda EGG (no BAM/P3M), PSX TMD (no HMD/TOD), COLLADA DAE (no ZAE), MD7 solo pose base.

## Integraciones de esta tanda

| ID | Vendor | Nivel verificado |
|---:|---|---|
| 81 | COLLADA `.dae` | 12 triángulos `box_nested_animation.dae`; `duck.dae` rechazado |
| 83 | MD4 | Compila, parse/skin enlazados; **sin muestra positiva** |
| 84 | MD7 | Compila, geometría pose base enlazada; **sin muestra positiva** |
| 85 | Panda3D EGG | Fixture sintético `triangle.egg` = 1 triángulo; BAM/P3M pendientes |
| 86 | PSX TMD | Compila, primitivas trianguladas; `empty.tmd` rechazado; **sin muestra positiva** |
| 87 | Unreal UC | `box.uc` parse correcto: metadatos de clase, referencias, secuencias; cero mallas por diseño |
| 88 | Unreal UE1 `.3d` | `box_d.3d` y `box_a.3d`: 12 triángulos y 30 frames |
| 89 | Source VTA | Fixture sintético: 1 hueso, 2 frames; no polígonos por diseño |
| 90 | Max Payne MP/KF | Compila, lector y visitor enlazados; **sin muestra externa positiva** |
| 91 | 3MF | `box.3mf` = 12 triángulos; **índice inválido aceptado**: requiere endurecimiento |

## Pruebas de regresión de proveedores antiguos

- FBX `box.fbx`: 12 triángulos, ID 1.
- MD2 `faerie.md2`: 654 triángulos, ID 45.
- OFF `Wuson.off`: 3732 triángulos, ID 40.
- `malformed_zero_numvertices.md2` e `invalid.off`: rechazados.

## Defectos y tareas que NO están solucionados

1. 3MF `malformed_property_index_oob.3mf` es indebidamente aceptado; fallo de validación.
2. 3MF `production_components.3mf` se rechaza; falta el soporte de componentes avanzado.
3. COLLADA `duck.dae` se rechaza por incompatibilidad/capacidad del parser.
4. VTA no aplica deltas al mesh externo; su estado nativo sí conserva los keyframes.
5. UC solo interpreta descriptores; no reconstruye automáticamente sus referencias `.3d`.
6. MD4, MD7, TMD, MP/KF siguen sin muestra positiva de su formato dentro de los corpus probados.
7. Panda3D BAM/P3M y PSX HMD/TOD siguen pendientes.
8. La exactitud de materiales/UV y animaciones de toda esta tanda requiere inspección visual adicional.

## Cómo repetir

```sh
make -j4 libmodel_runtime89.a
cc -std=c89 -pedantic-errors -Wall -Wextra -O2 -Iinclude tests/native_repair_regression.c libmodel_runtime89.a -o native_repair_test
./native_repair_test /ruta/a/box.3mf /ruta/a/box_d.3d /ruta/a/box.uc
```

**No mezclar un contador de proveedores registrados con formatos distintos ni con certificación de fidelidad.**
