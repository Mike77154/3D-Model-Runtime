# ModelRuntime89 v19 · Tres códecs nuevos sin perder los anteriores

## Registro estable: 14 proveedores

| ID | Proveedor | Estado |
|---|---|---|
| 1 | FBX | Sin reemplazar |
| 2 | XPS / XNALara | Sin reemplazar |
| 3 | GoldSrc / Half-Life 1 `hl1modelib` v7 | Sin reemplazar |
| 4 | MMD | Sin reemplazar |
| 5 | Genesis3D `.act` / `.bdy` / `.mot` | Sin reemplazar |
| 6 | Resident Evil 1 | Sin reemplazar |
| 7 | Resident Evil 3 | Sin reemplazar |
| 8 | DirectX `.x` | Sin reemplazar |
| 9 | Serious Sam Classic | Sin reemplazar |
| 10 | ActorX PSK/PSA | Sin reemplazar |
| 11 | Source / Half-Life 2 `hl2modelib` | Sin reemplazar |
| **12** | **MilkShape 3D `.ms3d`, codec v1.7** | **Añadido** |
| **13** | **Cinema 4D `.c4d`, codec v0.14** | **Añadido** |
| **14** | **Torque DTS `.dts`, codec fase 41** | **Añadido** |

Los directorios originales completos quedan en `vendor/` (fuentes, encabezados, documentación, pruebas y herramientas, excepto los productos de compilación).

## Integración real

- `src/providers/provider_ms3d89.c`: identifica `MS3D000000`, parsea sin conversiones intermedias, emite triángulos nativos por grupo (incluidos los huérfanos), materiales, rutas de textura, huesos y poses muestreadas con `ms3d_skin_evaluate_mode(...,MS3D_ROTATION_QUAT_SLERP)`. La API del proveedor mantiene SLERP, NLERP, el reproductor y el crossfade temporal para clientes nativos.
- `src/providers/provider_c4d89.c`: identifica XC4DC4D6/QC4DC4D6, recorre mallas nativas, emite triángulos y triangula quads con UV propios de cada esquina. Expone rutas de bitmap recuperadas por el módulo `texturer_opener`. Conserva `mesher`, `uv_texturer`, `shaderer`, `animer` y `texturer_opener` más el núcleo original.
- `src/providers/provider_dts89.c`: parsea DTS en memoria de arena, construye la vista nativa de LOD, expande las primitivas triangulares/strips/fans con la corrección de fase 41, emite geometría, UV, normales cuando están presentes y materiales. Las API originales de DSQ, skinning/poses, TorqueScript y shaderer quedan accesibles desde `vendor/dts89`.

**UV:** los archivos de estos tres formatos conservan su coordenada nativa `V` en las bibliotecas; **solo el adaptador al presenter OpenGL** invierte `V=65536-V` una vez (Q16.16). Se conserva tiling fuera de [0,1].

## Límites explícitos

1. `MS3D`: el visor puede muestrear la animación nativa SLERP, pero no hay controles de mezcla/crossfade entre dos rangos en el frontend. Eso está disponible en el vendor para los engines.
2. `C4D`: la versión de corpus C4D6 de este decodificador NO resuelve completamente `TextureTag → material BaseLink`, relaciones jerárquicas completas, ni pesos/skin inverso. El visor expone geometría, UV y referencias de bitmap, **no asume asignaciones de textura/material no demostradas**. Tracks escalares y smooth shaderer están vendorizados, pero no se afirma reproducción esquelética completa en el presenter.
3. `DTS`: el visor presenta geometría **estática LOD0** en la vista común; la biblioteca DTS preserva DSQ, animación, pose, selección automática de LOD y shaderer, pero su adaptación completa a los controles de animación del visor y el grafo de transformaciones de objetos aún son tareas pendientes. No debe confundirse registro del proveedor con renderización idéntica a Torque.
4. El presenter Win32 se entrega como **fuente**. Esta máquina Linux no dispone de MinGW32/Wine para certificar que el EXE corre gráficamente.
5. El formato `C4D6` no equivale a compatibilidad universal con todos los Cinema 4D modernos.

## Ejemplo para cualquier engine

```c
#include "model_runtime89.h"
#include "modelrt_native89.h" /* opcional, acceso sin pérdida */

static unsigned char arena[192u*1024u*1024u];
static modelrt_runtime runtime;
static modelrt_handle handle;

/* El engine crea sus propios callbacks modelrt_visit_sink. */
void abrir(const char *ruta) {
    modelrt_runtime_init(&runtime);
    modelrt_register_builtin_providers(&runtime);
    modelrt_handle_init(&handle,arena,sizeof(arena));
    if(modelrt_open_file(&runtime,&handle,ruta)==MODELRT_OK) {
        /* modelrt_visit(&handle,-1,0,&callbacks,usuario); */
        /* modelrt_native_ms3d(&handle) si es MS3D */
        /* modelrt_native_torque_dts(&handle) si es DTS */
    }
}
```

Compilar: `make -j4` en `model_runtime89/`; usar `libmodel_runtime89.a` con `-Iinclude` y (si se usa `modelrt_native89.h`) mantener los headers `vendor/` adjuntos.

Prueba de integración: `make v19-regression` después de generar con el make del codec MS3D su `vendor/giffany_ms3d_codec_c89/tests/tiny.ms3d` cuando no exista. El fixture DTS se genera dentro de la prueba; el fixture de MS3D se incluye.

El visor v19 vendorizó **la copia exacta** del runtime v19 en `vendor/model_runtime89`; `Makefile.mingw` llama al Makefile incluido, sin bifurcar registros.
