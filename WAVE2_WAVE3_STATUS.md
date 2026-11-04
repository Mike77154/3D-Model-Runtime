# Modelo Runtime Experimental — segunda ronda real de adaptadores

**Estado validado en esta entrega:** 29 proveedores registrados en `modelrt_register_builtin_providers()`.

- **14** proveedores originales ModelRuntime v19 intactos.
- **6** importadores GACKT añadidos en la versión anterior: OFF, OBJ, PLY, Irrlicht, STL, MD2.
- **9** importadores nativos implementados aquí, con código original vinculado al runtime:
  AC3D, 3DS, LightWave LWO, OpenGEX, RAW, NFF, Blitz3D B3D, Quick3D Q3O/Q3S, Gamestudio HMP/HMB.
- **50** vendors de GACKT siguen en `vendor/gackt_3dcodecs/` como código fuente pendiente; NO tienen importador ModelRuntime activo, y NO se etiquetan como completados.

## Cómo se ejecutan

Las implementaciones nuevas están en `src/providers/provider_gackt_wave2_89.c` y `src/providers/provider_gackt_wave3_89.c`.
Cada `open()` invoca directamente el parser original y asigna el resultado a `handle->native_state`.
Cada `visit()` publica geometría verdadera por callbacks. No hay passthrough ficticio,
no se ejecuta el proceso GACKT, no se escribe un asset universal y las fuentes originales del runtime no se reemplazan.

IDs 46–54, en ese orden, son los nueve nuevos. Sus estructuras propietarias están accesibles mediante
`modelrt_native_gackt_wave2_89` / `modelrt_native_gackt_wave3_89` (documentadas en `include/modelrt_native_gackt89.h`).

## Prueba C89 y corpus externo

- Biblioteca recompilada con `-std=c89 -pedantic-errors -Wall -Wextra -O2`: **PASS**.
- Enlazado y ejecución de `tests/experimental_corpus_check.c`: **29 proveedores reconocidos**.
- Selección de **113 archivos** de los dos ZIP de Assimp, incluyendo archivos malformados: **80 visitas con retorno cero**, **33 rechazos**.
- **66 archivos con triángulos no vacíos**. Los demás éxitos incluyen escenas vacías y datos no triangulados.
- La distribución por extensión y el detalle individual están en `EXPERIMENTAL_WAVE23_CORPUS_SUMMARY.csv` y `EXPERIMENTAL_WAVE23_ASSIMP_CORPUS.csv`.

### Casos concretos

- `jeep1.3ds`: **2032** triángulos; `pyramob.3DS`: **31460**.
- `SphereWithLight.ac`: **264** triángulos.
- `QuickDraw--Arm-ForeArm.lwo`: **392** triángulos; otros LWO rechazados.
- `Wuson.raw`: **3732** triángulos.
- `WusonBlitz.b3d`: **3732**; `turtle1.b3d`: **640**.
- `WusonOrange.q3o`: **3732**, `earth.q3o`: **2937**.
- `terrain.hmp`: **2048** triángulos.

## Límites conocidos y problemas pendientes

- Estas nueve conexiones no certifican el skinning ni la reproducción completa de animaciones.
- La vista común de geometría puede reagrupar submallas; el `native_state` preserva el modelo original.
  En particular los visitors de estas rondas son una vista geométrica inicial, no el renderer nativo final.
- OpenGEX: cuatro escenas devuelven éxito pero cero triángulos; **no aprobar como geometría** sin revisar sus estructuras.
- NFF: primitivas analíticas como esfera y cono se preservan como escena nativa; no hay teselación automática.
- LWO: cuatro de 16 archivos seleccionados fueron aceptados; hay variantes por ampliar.
- AC3D: UTF-8 BOM y UTF-16LE rechazados en la muestra; requiere capa de codificación si se desea.
- RAW con datos auxiliares de texturas/colores no se abre con el parser RAW tradicional: dos rechazos de corpus.
- B3D `dwarf2.b3d`, Q3D `E-AT-AT.q3o`: fallos de parsing en este corpus.
- Quedan 50 vendors sin conexión, enumerados con candidatos de API en `PENDING_VENDOR_PORTS_50.csv`.
- **No se realizaron pruebas completas de corrección visual, UV, animación y compatibilidad total.**

## Compilación de referencia

```sh
make -j4 libmodel_runtime89.a
cc -std=c89 -pedantic-errors -O2 -Iinclude tests/experimental_corpus_check.c libmodel_runtime89.a -o corpus_check
./corpus_check /ruta/a/modelo.3ds /ruta/a/otro_modelo.b3d
```

Las matrices usan modelos externos con licencia propia y no se redistribuyen dentro de este ZIP.
