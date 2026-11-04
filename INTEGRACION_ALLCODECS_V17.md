# Model Runtime 89 + Viewer — Vendorizado v17 (2026-10-08)

**C89 estricto, matemáticas de geometría/animación fixed-point**, sin conversión intermedia obligatoria de mallas. Paquete de código fuente; no contiene un EXE de Windows precompilado. La librería `model_runtime89` se puede enlazar en otros motores sin dependencia de Win32/OpenGL. El visor es el frontend opcional Win32/OpenGL 1.1 y contiene **su propia copia íntegra** del runtime.

## Códecs conservados + añadidos (10 proveedores)

| ID | Proveedor | Extensiones / función |
|---|---|---|
| 1 | FBX / GACKT | `.fbx`, geometría + clips y rigs compatibles |
| 2 | XPS / XNALara | `.xps`, `.mesh`, `.mesh.ascii` |
| 3 | GoldSrc / Half-Life 1 | `.mdl`, `.smd` |
| 4 | MMD | `.pmd`, `.pmx`, `.vmd`, `.vpd` |
| 5 | Genesis3D | `.act`, `.bdy`, `.mot` |
| 6 | Resident Evil 1 | `.emd`, `.emw`, `.tmd` |
| 7 | Resident Evil 3 | `.emd` (enrutamiento por contenido, con RE1) |
| 8 | DirectX `.x` | `.x`, text / bin / tzip / bzip según parser original |
| 9 **NUEVO** | Serious Sam Classic 89 v0.9 | `.mdl` vértices, superficies, materiales, frames, normals, UV; ruta nativa adicional para FX/skin |
| 10 **NUEVO** | ActorX PSK/PSA v3 rootfix | `.psk`, `.pskx` malla/rig + `.psa` banco de clips independiente, bone-map/skin CPU |

Las IDs anteriores **1-8 permanecen intactas**, solo se añaden IDs `9` y `10`. `.mdl` puede pertenecer a GoldSrc o Serious Sam; el runtime intenta proveedores compatibles y valida contenidos. La lista de proveedores es fija en el enlace y se registra llamando `modelrt_register_builtin_providers()`.

## Integración desacoplada para cualquier engine

Estructura del bundle (el ZIP del runtime contiene la carpeta model_runtime89):

```
ModelViewerRuntime89_AllCodecs_v17/
├── Makefile.mingw                      # Compila frontend Win32 contra la librería vendor
├── src/ + app/ + include/            # Visor OpenGL (solo frontend)
├── build_mingw32.bat
└── vendor/model_runtime89/            # Runtime INTEGRADO e INDEPENDIENTE
    ├── Makefile                        # Crea libmodel_runtime89.a
    ├── include/model_runtime89.h       # API universal del runtime
    ├── include/modelrt_native89.h      # Datos nativos/FX sin perder información
    ├── src/providers/                  # 10 adaptadores de formatos
    │   ├── provider_serioussam89.c     # Nuevo
    │   └── provider_pskpsa89.c        # Nuevo
    ├── vendor/                         # CÓDIGO COMPLETO de los 10 vendors
    │   ├── SeriousSamClassicCodec89_v0_9/
    │   └── pskpsa3d_c89_actorx_rootfix_v3_portable/
    ├── examples/engine_embed89.c       # Ejemplo portátil
    └── tests/integration/             # Fixtures + prueba reproducible
```

**Uso en otros motores:** vendorizas la carpeta `model_runtime89` (o el ZIP separado), compilas su `Makefile`, incluyes `model_runtime89.h` y enlazas `libmodel_runtime89.a` con tu ejecutable. `examples/engine_embed89.c` muestra la apertura y un callback de triángulos. Tú decides si los triángulos se envían a OpenGL, Direct3D9, GDI o un renderer propio.

### Construcción MSYS2 MinGW32 (Windows 32-bit)

En terminal **MSYS2 MINGW32**, con `gcc`, `make` y `ar`:

```sh
cd ModelViewerRuntime89_AllCodecs_v17
mingw32-make -f Makefile.mingw
./model_viewer_w32gl89.exe
```

En `cmd.exe` también puedes ejecutar `build_mingw32.bat`, que limpia y recompila runtime y visor. No añadas las bibliotecas nativas como objetos externos a la vez que el runtime: ya están dentro de `libmodel_runtime89.a`.

### Construcción independiente del runtime

```sh
cd model_runtime89  # ZIP separado, o cd vendor/model_runtime89 desde el visor
make -j4
cc -std=c89 -pedantic-errors -Iinclude examples/engine_embed89.c libmodel_runtime89.a -o example_engine
./example_engine tests/integration/fixtures/actorx_triangle.psk
sh tests/integration/run_test.sh
```

En MSYS2 MINGW32 usa `mingw32-make`; el ejemplo nativo requiere consola compatible. No requiere el EXE del visor.

### Modelos + animaciones separados

- Abre primero `personaje.psk` en el visor.
- Usa **Archivo > Cargar animacion... (Ctrl+L)** y selecciona `personaje.psa`.
- El adaptador mapea huesos por nombre (insensible a mayúsculas), samplea poses con SLERP, hace skinning de vértices y normales y reporta los clips. Los huesos sin correspondencia se registran; **no** hay retargeting anatómico automático.
- `.psa` aislado es una **fuente de animación sin malla**; es normal que no se vea un personaje al abrirlo como modelo.
- Las coordenadas ActorX Z-up se convierten a Y-up solo para emitir geometría a la API del runtime: datos nativos siguen intactos.

## Dos niveles de fidelidad

**Visita universal** (`modelrt_visit`, `modelrt_visit_bound_animation`): geometría, colores, normales, UV, materiales, clips. Serious Sam v0.9 también emite flags de material: bump, detail, reflection, specular, haze, fog, blend, alpha test. **Son metadatos/hints, no shaders completos**.

**Acceso nativo** (`include/modelrt_native89.h`): `modelrt_native_serioussam(handle)`, `modelrt_native_psk(handle)`, `modelrt_native_psa(handle)` entregan punteros de solo lectura a structs nativos. El runtime compila las implementaciones nativas Serious Sam `ss89fx`, `ss89fxpasses`, `ss89env`, `ss89resources`, `ss89skin`. Esto permite que un engine implemente sus propios render passes, ensamblaje AMC, carga de `.tex`, efectos y rig avanzado **sin recortar el codec vendor original**.

### Límites explícitos

- **No** se ha implementado en el frontend OpenGL 1.1 la reproducción completa de todos los pases de materiales/sombras/decal/reflexiones del motor Serious Engine. Se conserva el código nativo como vendor, pero `ModelViewer` solo utiliza los atributos comunes ya soportados por su presenter.
- Los `.amc` (attachments), `.bm/.skl/.ba` (Serious Sam rig avanzado) y `.tex` están disponibles en el árbol vendor original; **no** se registran como modelos universales independientes mediante el nuevo adaptador, ni se promete carga visual de jerarquías complejas desde el menú Abrir. También puede ser necesaria la conversión/carga manual de texturas nativas `.tex` para el viewer.
- La implementación universal de PSK soporta skinning de huesos y normales; morphs y metadatos PSKX existen en el vendor pero no todos se aplican automáticamente desde el frontend del visor.
- No se verificó **el EXE de Windows** aquí, porque este entorno no dispone de compilador MinGW ni ejecutor Wine. Sí se construyó la librería en Linux con `-std=c89 -pedantic-errors`; la prueba de integración y las regresiones indicadas abajo pasaron.
- La API de triángulos no impone un engine determinado. Renderers que quieren efectos especiales completos deben interpretar los metadatos o consultar la API nativa.

## Verificación en Linux (efectuada)

- `make -j4`: construyó `libmodel_runtime89.a` con los 10 proveedores.
- `sh tests/integration/run_test.sh`: regresión Serious `.mdl` y PSK + PSA, 2 fotogramas con cambio real de vértice.
- Regresiones originales: provider-fallback, GoldSrc, XPS, FBX+animación externa, DirectX `.x`, RE1, RE3 e imagen PNG/BMP/DDS/TGA/JPG/TIM/TIFF: pasaron.
- `make check` nativo ActorX y pruebas de módulos nativos Serious FX, FXpasses y environment v0.9: pasaron.

Los fixtures PSK/PSA son triángulos sintéticos de prueba y no modelos de juego. Los assets del usuario que no estén dentro de estos ZIP no se pueden suponer presentes. Se conservaron todas las carpetas completas de vendors originales y la jerarquía del visor.
