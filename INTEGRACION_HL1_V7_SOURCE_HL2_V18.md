# ModelRuntime89 v18 / ModelViewer Win32GL89 v18

## Cambio solicitado

- **Sustitución** del proveedor Half-Life 1 (se eliminó `src/providers/provider_hl1_89.c`, así como el árbol de su biblioteca anterior). Se incluye el `hl1modelib_SubpixelTopLeft_v7` aportado por el usuario bajo `vendor/hl1modelib/`, compilado por `provider_hl1modelib89.c`.
- **Alta** de `hl2modelib_v1_0_24` bajo `vendor/hl2modelib/` con sus 26 unidades de implementación y código fuente íntegro; el adaptador `src/providers/provider_hl2modelib89.c` conecta los datos nativos directamente al contrato de visitantes ModelRuntime.
- **Permanecen** FBX, XPS/XNALara, DirectX `.x`, MMD, Genesis3D `.act/.bdy/.mot`, RE1, RE3, Serious Sam Classic y ActorX PSK/PSA. Total: **11 proveedores**.

## Enrutamiento de archivos compartidos

Ambos sistemas usan `.mdl` y el magic `IDST`. No basta mirar la extensión:

- GoldSrc/HL1: `IDST` con entero little endian versión **10**.
- Source/HL2: `IDST` con versión **44..60** en este adaptador (la biblioteca conserva sus propias comprobaciones de compatibilidad).
- Serious Sam Classic `.mdl`: prueba de su propio codec nativo, sin colisionar con `IDST`.
- `.smd` genérico: GoldSrc por compatibilidad del runtime existente. Para forzar Source use `*.source.smd` o `*.hl2.smd`. Son archivos SMD originales renombrados, NO conversión ni formato intermedio.

## Fuente HL2: geometría

El adaptador abre el `.mdl` y necesita a su lado `.vvd` y uno de estos archivos: `.dx90.vtx` (primero), `.vtx`, `.dx80.vtx`, `.sw.vtx`, `.dx11.vtx`. Se valida sincronía / checksum MDL+VVD+VTX.

Se expone `hl2m_engine_model` original mediante `modelrt_native_source_engine_mesh()` y el visitante `modelrt_visit()` entrega triángulos, UV, normales, material, hasta 3 pesos por vértice y huesos. Se utiliza LOD0. Las funciones nativas de LOD, physique, pose controllers, autolayers, decompilación, etc. permanecen en el proveedor completo, accesibles directamente desde el motor, **pero no todas están trasladadas a controles del visor**.

Opcional `.ani` del mismo basename se carga si existe. `modelrt_visit()` intenta muestrear las secuencias Source o las animaciones locales; el skinning en el puente se realiza en fixed-point mediante matrices de bind inversa y animación. No se aplica un fallback falso si el codec no soporta una secuencia. Los clips que requieren `.ani` ausente, dependencias externas, virtual models de varias bibliotecas o parámetros específicos pueden necesitar la API nativa de `hl2modelib`.

## Texturas Source

`modelrt_texture_info_get()` busca bajo carpetas `materials/` a partir del directorio del modelo y antecesores: consulta nombres de materiales y directorios `cdtextures` del MDL; lee un `.vmt` si lo encuentra y sigue su `$basetexture` al `.vtf` correspondiente. Cuando encuentra un VTF compatible, notifica `MODELRT_TEXTURE_NATIVE_FILE` y `modelrt_texture_decode_rgba()` llama al decoder del proveedor (BC1/BC3 y los formatos sin compresión que éste implementa). El presenter OpenGL invierte filas para el upload nativo.

**Limitaciones**: resolución basada en rutas de disco, no VPK de Steam montados; VMT complejos (proxies, resoluciones indirectas), VTF cubemap/volume/formatos no soportados pueden necesitar un resolver adicional. El shaderer y las utilidades de materiales nativas quedan incluidos, pero la reproducción de Source shaders completos no está prometida por el renderer OpenGL 1.1.

## Memoria y portabilidad

C89 estricto, sin malloc/free desde los adaptadores, sin float/double/64-bit en el camino nuevo, datos en arena de workspace propiedad del engine. **Importante**: por los arrays máximos de `hl2m_engine_model`, la carga de MDL+VVD+VTX alcanza aprox. 120 MiB de reserva pico en este proveedor, aun con modelos pequeños. Ofrezca una arena de **al menos 176 MiB** para Source; el visor incrementa `VIEWER_MODEL_WORKSPACE_BYTES` a 176 MiB. Se liberan buffers auxiliares lógicos después del ensamblado, pero el tamaño de `hl2m_engine_model` es intencionalmente grande (configuración original del códec completo).

Las estructuras del codec tienen límites declarados. Si se agotan, se reporta error, no se recorta una malla silenciosamente. Los APIs de acceso nativo devuelven punteros prestados mientras el handle esté abierto. Para compartir entre engines: usar `#include "model_runtime89.h"` y opcionalmente `modelrt_native89.h`. Los motores pueden usar directamente las cabeceras `vendor/hl2modelib/include/hl2m_master89.h` y `vendor/hl1modelib/hlfp.h`.

## Compilación

**Runtime independiente**:

```
cd model_runtime89
make -j4
make hl2modelib-regression goldsrc-regression goldsrc-axis-regression
```

Para ver los 11 códecs, llamar a `modelrt_runtime_init`, `modelrt_register_builtin_providers`, `modelrt_open_file`, `modelrt_visit`. Ver `examples/engine_embed89.c` y `examples/engine_embed_source89.c`.

**Visor Windows (MSYS2 MinGW32)**:

```
cd ModelViewerRuntime89_AllCodecs_v18
./build_msys2_mingw32.sh
```

Usa `Makefile.mingw`, compilando desde `vendor/model_runtime89/`; conserva renderer Win32/OpenGL. El archivo `app/winmain.c` inicia el visor.

## Validación aquí

- Biblioteca integrada compilada en Linux GCC con `-std=c89 -pedantic-errors -Wall -Wextra -Werror`.
- 20 blancos de regresión del runtime pasaron (incluye el nuevo de Source).
- Source fixture: MDL+VVD+VTX 1 triángulo, SMD 1 triángulo, y VMT -> VTF BC1 4×4 RGBA8.
- HL1 v7: regresiones de GoldSrc, ejes y máscaras pasan.
- **No se compiló ni ejecutó aquí el .exe Win32** por falta de toolchain MinGW32/Wine. No se afirmó validar ejecución gráfica en Windows.
- **No se probó en el puente** un corpus de modelos Source reales con secuencias complejas, physbones, flexes y virtual-model montado. Los tests de paquete confirman decodificación/visita básica, no fidelidad total del motor Source.
