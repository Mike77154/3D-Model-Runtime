## RE4 Classic v4.1 (new additive provider 104)

Resident Evil 4 Classic v4.1 code is vendored in `vendor/re4_classic_v4_1/`.
Native PMD/BIN bind-pose meshes, TPL textures and FCV channel inspection are
supported. **NEW:** explicit PMD+FCV attachment now exposes native animated
vertex streams using the codec's fixed-point FK, weighted or rigid skinning.
For multipart characters, `modelrt_re4_visit_character_frame` supports audited
extra PMDs with strict bone matches and independent texture ownership.
No asset search, implicit clip selection, or synthetic FPS. See
`docs/RE4_CLASSIC_V41_RUNTIME.md` and
`docs/RE4_CLASSIC_ANIMATED_CHARACTER_RUNTIME.md`.

> **2026-11-01 — HOD2 Phase12**: tablas H2TB independientes de ejecutables, consulta de 86 esqueletos por perfil (1999/2000) desde el Runtime 75. Véase `docs/HOD2_PHASE12_STANDALONE_RUNTIME.md`.

# model_runtime89

Runtime multiformato de modelos 3D para C89. La regla central es deliberada: **no existe un formato universal obligatorio dentro del runtime**.

Cada codec conserva su representación nativa y el runtime sólo ofrece:

- detección/probing de formato;
- registro de providers;
- `modelrt_handle` con estado nativo opaco;
- bitset de capabilities;
- vistas transitorias (`triangle`, `material`, `bone`, etc.) mediante visitor callbacks;
- consulta de animaciones y texturas cuando el provider puede exponerlas;
- workspace entregado por el host; el runtime no llama `malloc`, `calloc`, `realloc` ni `free`;
- coordenadas y tiempo en Q16.16.

## Arquitectura

```text
archivo nativo
    |
    v
modelrt_open_file()
    |
    +-- probe registry ---------------------------------------+
    |                                                         |
    +--> FBX provider ------> estado FBX nativo               |
    +--> XPS provider ------> estado XPS nativo               |
    +--> DirectX .x provider -> xparsermodget98 v0.5 nativo     |
    +--> GoldSrc provider --> MDL/SMD nativo                  | no GenericModel
    +--> MMD provider ------> PMD/PMX/VMD/VPD nativo          | obligatorio
    +--> Genesis3D provider -> ACT/BDY/MOT nativo             |
    +--> RE1 provider ------> EMD/EMW/TMD (RE1) nativo          |
    +--> RE3 provider ------> EMD 15-section/MD2/EDD/EMR nativo |
                                                              |
                              modelrt_visit() <---------------+
                                    |
                                    v
                       vistas transitorias/callbacks
                                    |
                      renderer / editor / inspector
```

Las vistas son una interfaz de lectura. No son una copia persistente del modelo ni una etapa de conversión.

## Formatos y estado actual

| Familia | Extensiones | Geometría | Esqueleto/skin | Animación | Texturas | Notas |
|---|---|---:|---:|---:|---:|---|
| FBX | `.fbx` | sí | bones `LimbNode`/`Root`, jerarquía, pesos y CPU skinning | clips/transform sampling | rutas externas + blob embebido prestado | conserva escena FBX; cámaras/luces/morphs se anuncian por capability |
| XPS | `.xps`, `.mesh`, `.mesh.ascii` | sí | sí | según contenido del codec | rutas externas | parser callback/no-heap |
| DirectX X | `.x` (`txt/bin/tzip/bzip`) | sí | Frames/SkinWeights sin anatomía inferida | Multi-clip, frame count/ticks/timing; pose sampler cuando targets resuelven | `TextureFilename` externo | `xparsermodget98 v0.5 multiclip-frame-pose`; geometría y animación se capturan en pasadas caller-owned |
| GoldSrc | `.mdl`, `.smd` | sí | sí | secuencias MDL; frames SMD detectados | MDL embebida RGBA; SMD externa | bake de mesh con matrices nativas |
| MMD | `.pmd`, `.pmx`, `.vmd`, `.vpd` | PMD/PMX | PMD/PMX/VPD | VMD se abre como asset nativo | rutas externas | VMD no se fuerza dentro de un PMX; no hay unión artificial |
| Genesis3D | `.act`, `.bdy`, `.mot` | ACT/BDY | sí | ACT/MOT | bitmaps embebidos decodificables | LOD y eventos declarados por capability |
| Resident Evil 1 PSX | `.emd`, `.emw`, `.tmd` | sí, piezas TMD rígidas | jerarquía real EMR/PartList | Bank A + Bank B (EDD/EMR) | TIM embebido, CBA/TSB por primitive | `RE1codec v0.3-mvref`; Bank A queda unbound por política del runtime |
| Resident Evil 3 PSX | `.emd` | sí, body MD2 + aux MD2 | jerarquía rígida sólo con match exacto | A, B, Damage, Weapon A/B | slots externos CLUT/page | `RE3_codec v0.28.0`; header de 15 secciones distingue RE3 de RE1 |

### Importante sobre el alcance

El runtime ya abre/visita las geometrías soportadas por los providers anteriores, pero **no afirma paridad total de todas las características de todos los formatos**. En particular:

- FBX ya expone jerarquía de bones incluso cuando el armature está descrito por `Model::LimbNode/Root`; si hay clusters de skin, el provider evalúa posiciones skineadas en CPU al visitar el modelo;
- las texturas externas de FBX/XPS/MMD se exponen por ruta y FBX también puede prestar el payload codificado de media embebida mediante `modelrt_texture_encoded_view_get()`; el runtime no obliga a decodificar imágenes;
- VMD/VPD son assets independientes; no se adhieren automáticamente a un PMX/PMD;
- el provider RE usa ahora `RE1codec v0.3-mvref`: TMD, jerarquía EMR, clips EDD y TIM embebido forman una sola vista RE1 nativa. Cada combinación CBA/TSB se conserva como material/textura para no mezclar páginas o CLUT distintas.

Eso es intencional: una capability sólo se anuncia cuando existe una ruta real en el provider.

## FBX hardening (skeleton + textures)

Esta revisión corrige tres fallos del primer bundle:

1. el skeleton ya no depende exclusivamente de Cluster deformers: también reconoce `LimbNode`, `Root` y `Skeleton`;
2. `parent_index` se resuelve en una segunda pasada después de registrar todos los IDs de hueso, por lo que un hijo serializado antes que su padre conserva la jerarquía;
3. el provider FBX expone media embebida como una vista de bytes codificados prestada. No crea un formato de textura universal ni copia persistente.

El provider conserva además `texture_info.path` para texturas externas, permitiendo que un host (por ejemplo el viewer Win32) resuelva la imagen junto al FBX, en una carpeta `.fbm`, o usando la ruta relativa exportada.


## Built-in texture decoders

`model_runtime89` now includes an optional, default-enabled image decoding layer so model providers can resolve external or embedded textures without requiring the rendering backend to decode them. The vendored stack is `PNG89 + zlib89`, `BMP89`, `DDS89`, `TGA89`, `JPG89`, `TIM89`, and `TIFF89`.

`modelrt_texture_decode_rgba()` keeps provider-native decoders first, then tries an encoded embedded texture through the built-in registry, then resolves external texture paths and decodes them in the runtime. `modelrt_image_decode_memory()` and `modelrt_image_decode_file()` expose the same registry directly.

All image decoders are enabled by default but have both compile-time switches (`IMAGE_PNG`, `IMAGE_BMP`, `IMAGE_DDS`, `IMAGE_TGA`, `IMAGE_JPG`, `IMAGE_TIM`, `IMAGE_TIFF`) and a runtime decoder mask. See [`IMAGE_DECODERS89.md`](IMAGE_DECODERS89.md) for scope, limits, memory requirements, and tests.

## API mínima

```c
#include "model_runtime89.h"

static unsigned char workspace[32u * 1024u * 1024u];

static int on_triangle(void *user, const modelrt_triangle_view *tri)
{
    (void)user;
    /* Consumir tri sin almacenarlo en un GenericModel. */
    return 0;
}

int open_and_visit(const char *path)
{
    modelrt_runtime rt;
    modelrt_handle model;
    modelrt_visit_sink sink;
    int rc;

    modelrt_runtime_init(&rt);
    modelrt_register_builtin_providers(&rt);
    modelrt_handle_init(&model, workspace, sizeof(workspace));

    rc = modelrt_open_file(&rt, &model, path);
    if (rc != MODELRT_OK) return rc;

    memset(&sink, 0, sizeof(sink));
    sink.on_triangle = on_triangle;
    rc = modelrt_visit(&model, MODELRT_ANIM_BIND_POSE, 0, &sink, 0);
    modelrt_close(&model);
    return rc;
}
```

## Clips embebidos y fuentes de animacion externas

El runtime separa dos conceptos que **no se convierten a un formato universal**:

1. **clips embebidos**: pertenecen al handle del modelo (`modelrt_animation_count()` / `modelrt_animation_info_get()`);
2. **fuente externa de animacion**: se abre como un segundo `modelrt_handle` y se vincula temporalmente al modelo mediante `modelrt_animation_binding`.

Ejemplo de consulta de clips embebidos:

```c
int i;
int count;
modelrt_animation_info ai;

count = modelrt_animation_count(&model);
for (i = 0; i < count; ++i) {
    if (modelrt_animation_info_get(&model, i, &ai) == MODELRT_OK) {
        /* ai.name, ai.start_time, ai.end_time */
    }
}
```

Ejemplo de attachment sin heap:

```c
static unsigned char anim_workspace[32u * 1024u * 1024u];
static unsigned char bind_workspace[2u * 1024u * 1024u];
modelrt_handle anim_source;
modelrt_animation_binding binding;

modelrt_handle_init(&anim_source, anim_workspace, sizeof(anim_workspace));
modelrt_animation_binding_init(&binding, bind_workspace, sizeof(bind_workspace));

modelrt_open_file(&rt, &anim_source, "walk.fbx");
if (modelrt_animation_bind(&binding, &model, &anim_source) == MODELRT_OK) {
    const modelrt_animation_binding_info *bi;
    bi = modelrt_animation_binding_get_info(&binding);
    /* bi->matched_track_count / bi->source_track_count sirve como diagnostico. */
    modelrt_visit_bound_animation(&binding, 0, 0, &sink, user);
}
```

Los dos handles deben permanecer abiertos mientras exista el binding. El workspace de binding es entregado por el caller y no usa `malloc`.

### Attachment implementado en esta revision

| Target | Fuente | Estrategia |
|---|---|---|
| FBX con rig | FBX con AnimationStack/curvas | remapeo de tracks por nombre completo y fallback por nombre hoja; las curvas siguen perteneciendo al FBX fuente |
| Genesis3D `BDY`/`ACT` | `.MOT` o banco `.ACT` | paths del motion se casan contra nombres de bones del body y se usa el sampler nativo Genesis3D |

El attachment FBX actual cubre transformacion esqueletal/TRS. No pretende hacer retargeting anatomico entre rigs distintos ni trasladar automaticamente morph animation entre personajes. Si los nombres/rigs no casan, `matched_track_count` permite detectarlo en vez de fingir compatibilidad.

Los providers que ya contienen animacion embebida siguen exponiendola por la API generica; por ejemplo FBX expone sus clips/stacks, Genesis3D ACT sus motions y GoldSrc MDL sus secuencias.

## Compilar

### MSYS2/MinGW32 o GCC compatible

```sh
make
```

Build estricto usado para verificación:

```sh
make clean
make CFLAGS='-std=c89 -pedantic-errors -Wall -Wextra -O2'
make smoke CFLAGS='-std=c89 -pedantic-errors -Wall -Wextra -O2'
./tests/smoke vendor/3dfbxplus/test_quad.fbx
```

## Vendor tree

```text
vendor/
├── 3dfbxplus/
├── xps_uvfix_work/
├── hl1modelib/
├── mmd_codec_c89_fixed_v3/
├── mmd89_anim_codec/
├── genesis3d_c89_codec/
├── re1codec_v0_3_mvref/
├── re3_codec_v0_28/
├── xparsermodget98_v0_5/
├── zlib89/
├── png89/
├── bmp89/
├── dds89/
├── tga89/
├── jpg89/
├── tim89/
└── tiff89/
```

`recodec_c89` fue retirado de este bundle. `provider_re89.c` adapta directamente `RE1codec v0.3-mvref`, conservando el contrato no-heap/fixed-point. La sustitución es deliberadamente RE1-only: `.emd`, `.emw` y `.tmd` standalone se abren con `re1c_open_model()`. Las familias RE2 del provider antiguo (`.pld`, `.plw`, `.md1`, `.md2`) permanecen fuera de este provider. RE3 vuelve como provider separado mediante `RE3_codec v0.28.0`, sin reutilizar el reader genérico retirado.

`mmd89_anim_codec` se conserva como codec especializado complementario. El provider principal usa el codec MMD combinado para PMD/PMX/VMD/VPD; no se duplica el mismo parser en el binario sólo por estar vendorizado.

## Contrato de memoria

- el host entrega `workspace` a `modelrt_handle_init`;
- los providers obtienen memoria únicamente del arena lineal del handle;
- `modelrt_close()` invalida el estado nativo; reinicializar el handle reutiliza el buffer;
- las vistas entregadas al visitor no deben guardarse como punteros persistentes.

## Licencias

El código de integración de `model_runtime89` se entrega como parte de este bundle. Los paquetes bajo `vendor/` conservan sus archivos de licencia y avisos originales; no se re-licencian automáticamente por estar vendorizados.

## GoldSrc correction

This revision switches MDL animation sampling to the GoldSrc-compatible quaternion path and adds `make goldsrc-regression`. See `GOLDSRC_VIEW_FIX.md`.

## GoldSrc transparency / render-mode views

The GoldSrc provider now maps native Studio texture flags onto portable
`MODELRT_MAT_*` render semantics in both `modelrt_material_view.flags` and
`modelrt_texture_info.flags`. In particular, `STUDIO_NF_MASKED` becomes
`MODELRT_MAT_ALPHA_MASK`; the existing native palette decoder turns index 255
into alpha 0. See `GOLDSRC_TRANSPARENCY.md` and run:

```sh
make goldsrc-transparency-regression
```

## XPS UV mapping

The XPS provider preserves native codec UV data and adapts the renderer-facing
view using the UV layer declared by the selected XPS texture. Its V coordinate
is converted with `1 - V` for the viewer convention. UV values are not clamped,
so intentional repeat/tiling outside the 0..1 interval remains intact.
See `XPS_UV_FIX.md` and `tests/xps_uv_regression.c`.

## Resident Evil 1 / RE1codec v0.3-mvref

Esta revision sustituye por completo el reader RE anterior por `vendor/re1codec_v0_3_mvref`. El codec nuevo conserva las seis secciones nativas del EMD (`EMR_A`, `EDD_A`, `EMR_B`, `EDD_B`, `TMD`, `TIM`) y el provider las presenta sin crear un modelo universal persistente.

- cada objeto TMD se visita como pieza rigida;
- la jerarquia y pivotes salen de `EMR PartList`;
- los clips de Bank A y Bank B se enumeran como animaciones embebidas;
- el reloj del viewer mapea `step_units` a 30 ticks/s como politica del host, sin modificar el dato nativo;
- cada combinacion `CBA/TSB` observada en primitives soportadas recibe un material y una vista RGBA 256x256 derivada del TIM embebido;
- color PSX 0 se expone con alpha 0 y el material usa `MODELRT_MAT_ALPHA_MASK`;
- el provider conserva rigidez por pieza mediante `bone[0]=object_index`, peso 1.0.

Regression:

```sh
make re1codec-regression
```

El fixture sintetico prueba apertura EMD, pieza TMD, jerarquia, dos banks de clips y decode TIM sin depender de assets comerciales.


### RE1 v0.3-mvref: jerarquia rigida, no skeleton generico

El provider RE1 no anuncia `MODELRT_CAP_SKELETON` ni `MODELRT_CAP_SKINNING`.
Expone `MODELRT_CAP_RIGID_HIERARCHY` y `modelrt_rigid_part_view`: la topologia
visible proviene exclusivamente del `EMR PartList` de **Bank B**. Bank A se
conserva y enumera como metadata de animacion nativa, pero queda
`MODELRT_ANIM_UNBOUND`: nunca se auto-mapea por ordinal a las piezas TMD.
`TMD object_count` y
`EMR object_count` se conservan como cantidades nativas separadas. No se
rellenan partes ni se infiere anatomia humana.

El codec v0.3 tambien aporta timing RE1MV confirmado (25 FPS / 40 ms por
frame-ref), FT3/FT4 variable-length packets, banks opcionales, EMD B-only, EMW
y TMD standalone. Los FT4 se triangulan solo en la vista transitoria del
renderer (0,1,2 + 0,2,3); el packet nativo de cuatro lados permanece en el
codec.

Cuando un EMW/TMD no trae TIM, cada CBA/TSB se publica como un slot de textura
externa sin filename inventado. El host puede asociarle una textura manual.


## Resident Evil 3 / RE3_codec v0.28.0

RE3 convive con RE1 como provider independiente. Ambos usan `.emd`, por lo que el probe RE3 valida la cabecera nativa (`DataCount == 15`) antes de que el probe RE1 por extensión pueda aceptar el archivo.

El provider expone body MD2, MD2 auxiliar, FT3/FT4, UV, CLUT/page y cinco bancos EDD/EMR. Un banco sólo puede transformar una mesh cuando su jerarquía está resuelta y `joint_count` coincide exactamente con `part_count`; los demás clips permanecen visibles como `MODELRT_ANIM_UNBOUND`. No existe fallback humanoide.

Las texturas no vienen incluidas en este codec, así que cada combinación CLUT/page se expone como slot externo sin ruta inventada. El viewer puede asignar archivos con su inspector de texturas. La ruta DAT/vertex-binding del codec se conserva nativa, pero este primer adapter de runtime no la promociona todavía a skinning universal.

Regression:

```sh
make re3-regression
```

## DirectX `.x` / xparsermodget98 v0.5

`vendor/xparsermodget98_v0_5` se integra mediante `provider_x89.c` como `MODELRT_CODEC_DIRECTX_X`. El provider no convierte el archivo a un formato universal: mantiene pasadas separadas de geometría y animación. La capa `xp_anim_pose` cuenta primero nodes/clips/tracks/keys, recibe storage del arena del handle y reconstruye poses densas bajo demanda.

La integración expone Mesh, polígonos triangulados en visita, MeshNormals, MeshTextureCoords, MeshVertexColors, MeshMaterialList/Material, `TextureFilename`, jerarquía `Frame`, `FrameTransformMatrix` y pesos de `SkinWeights`. Los nombres de Frame se resuelven tal como vienen en el archivo; no existe fallback humanoide.

Los `AnimationSet` exponen clips, source keys, ticks y frames densos. Cuando todos sus targets resuelven contra Frames reales, el provider anuncia `MODELRT_ANIM_POSE_SAMPLER` y puede muestrear la pose. `modelrt_visit()` sigue aceptando tiempo cuando existe `AnimTicksPerSecond`; para clips sin tick-rate, `modelrt_visit_animation_frame()` permite visitar directamente cualquier frame recuperado sin inventar FPS. Tracks unresolved/ambiguous permanecen `MODELRT_ANIM_UNBOUND`.

Pruebas: `make directx-x-regression`, `make directx-x-open-matrix`, `make directx-x-v05-animation` y `make directx-x-v05-frame-visit`. La última verifica reproducción por frame de un clip sin `AnimTicksPerSecond`.


## Nuevos vendors: Serious Sam v0.9 + ActorX v3

Ver `INTEGRACION_ALLCODECS_V17.md` y `tests/integration/run_test.sh`.

## Quake 1 alias MDL corpus patch (Runtime 75)

The Quake IDPO v6 provider (ID 71) has been corpus-tested on 117 authenticated
`.mdl` files. It supports seam-aware UVs, Quake-table vertex normals, grouped
vertex poses, and indexed skin images. Accurate RGB skin conversion requires a
separately supplied `gfx/palette.lmp`, not included here.

See `docs/QUAKE1_MDL_CORPUS117_C89.md` and
`docs/QUAKE1_CORPUS117_RESULTS.csv` for the test methodology and full results.
