# Modelo Runtime Experimental — Tanda 02 (Native Wave 6)

## Estado de la integración

- **55 proveedores registrados:** 14 originales + 41 vendors distintos de GACKT conectados, entre ellos 10 añadidos ahora (IDs 71–80).
- **24 vendors adicionales quedan en estado fuente vendorizada sin conexión.**
- Las dos bibliotecas BVH independientes usan la misma extensión `.bvh`, de modo que hay 10 proveedores y **9 nuevas familias/dialectos** de formato, no diez extensiones exclusivas.
- Solo se llaman funciones de los códecs C89 vendorizados y callbacks `modelrt_visit()`. Sin parser de GACKT-LANG, sin `gackt_passthrough_open_profile`, sin `.gmesh` obligatorio.
- La biblioteca de fuentes compiló mediante `make clean && make -j8 libmodel_runtime89.a` usando GCC `-std=c89 -pedantic-errors -Wall -Wextra -O2`.
- No se entregan binarios, PNG ni modelos del corpus en el ZIP.

## Nuevos proveedores registrados

| ID | Vendor | Datos entregados | Prueba con el corpus |
|---:|---|---|---|
| 71 | Quake MDL | mallas, UV y fotogramas de vértices | `steg.mdl` 305 tri; `gijoe.mdl` 408; `tekmechbot.MDL` 1748 |
| 72 | Wolfenstein MDC | mallas, normales, UV y fotogramas | `spider.mdc` 1368 tri, 250 frames; inválido rechazado |
| 73 | X3D | triangulación de mallas explícitas | `ChevyTahoe.x3d` 1106 tri; X3D de primitivas pendientes |
| 74 | LXO / Modo | datos nativos al pasar decodificación | dos muestras LXO rechazadas por decoder (`LXO3D_E_TRUNCATED`) aunque analizó puntos/polígonos; **no certificado** |
| 75 | XGL/ZGL | malla + UV y normales; gzip o zlib | `sample_official.xgl` 24 tri; `Spider_ascii.zgl` 1312; `Wuson.zgl` 3732 |
| 76 | DXF | polígonos 3D a triángulos | `PinkEggFromLW.dxf` 576 tri; `wuson.dxf` 3732; `lineTest.dxf` 4 tri |
| 77 | Softimage XSI | mallas con UV y normales cuando presentes | enlazado, **sin muestra XSI en estos corpus** |
| 78 | IFC | IFC4 triangulado vía `ifc3d` | `AC14-FZK-Haus.ifc` rechazado; **no certificado** |
| 79 | Anima Biovision BVH | huesos y secuencias originales | `01_01.bvh` 38 huesos, 2752 frames; `01_03` 4511; `Boxing_Toes` 3069 |
| 80 | Bravure BVH | huesos y poses | prueba dirigida de `01_01.bvh`: 31 huesos, código 0; queda detrás de Anima en el selector automático |

## Limitaciones conocidas

- Tres vendors están **registrados pero no certificados** por el corpus: LXO, XSI e IFC. Su mera presencia en el registro no equivale a decodificación exitosa.
- X3D recupera mallas explícitas, pero su ruta nativa de triangulación no cubre aún todos los primitivos e instancias. `HelloWorld.x3d` deja de reportar éxito falso con cero triángulos.
- LXO detecta geometría en ambas muestras de Assimp, pero el parser upstream informa que el flujo es truncado; no se ha suprimido el error para forzar un éxito.
- XGL/ZGL añade ruta zlib usando `zlib89` ya vendorizado en Runtime89, sin dependencias nuevas ni heap.
- La compilación BVH usa `ANIMA_BVH_MAX_FRAMES=5000` y `BRAVURE_BVH_MAX_FRAMES=5000`; archivos mayores se rechazan. Los estados nativos de BVH consumen considerable memoria en arenas del llamador.
- `.bvh` usa el primer proveedor aceptado según el registro; Bravure es el segundo candidato y su implementación se probó directamente. No se fusionan dos códecs en uno.
- No se verificó fidelidad visual completa de todas las animaciones, texturas o materiales, ni velocidad frente a Assimp.
- Se conservaron los proveedores previos, probando regresiones seleccionadas de FBX, MD2, OFF, Irrlicht, MD5 y AMF. Un MilkShape malformado fue rechazado como corresponde.

## API y archivos

- `src/providers/provider_native_batch6a_89.c`: Quake MDL, MDC, X3D, LXO, XGL/ZGL
- `src/providers/provider_native_batch6b_89.c`: DXF, XSI, IFC, Anima BVH, Bravure BVH
- `include/modelrt_native_batch6_89.h`: acceso tipado a datos originales, prestados durante la vida del handle
- `tests/native_batch6_regression.c`: prueba API completa de 55 proveedores contra archivos suministrados por el usuario; requiere rutas como argumentos
- `tests/native_batch6_bravure_direct.c`: prueba dirigida del segundo proveedor BVH
- `BATCH6_CORPUS_RESULTS.csv`: matriz de resultados conservada
- `PENDING_VENDOR_PORTS_24.csv`: lista de integraciones futuras, sin falsos positivos

## Compilación

```sh
make clean
make -j8 native-batch6-build
cc -std=c89 -pedantic-errors -Wall -Wextra -O2 -Iinclude \
  tests/native_batch6_regression.c libmodel_runtime89.a -lm -o native_batch6_regression
./native_batch6_regression /ruta/corpus/modelo.mdl /ruta/corpus/modelo.x3d
```

La distribución ZIP contiene únicamente código fuente y documentación, **no contiene ejecutables**. El harness usa memoria estática para verificar el estilo sin heap del runtime.
