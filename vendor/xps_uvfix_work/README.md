# xpsimport_c89_fixed v4

Librería de importación **XNALara / XPS** orientada a **C89**, **sin `malloc`** y con números en **Q16.16**.

## Qué trae este ZIP

- detección de formato: `.mesh.ascii` / `.ascii`, `.mesh`, `.xps`, `.pose`, `BoneDict.txt`, `.scene`
- importación por **callbacks** para:
  - modelos (`xps_sink`)
  - poses (`xps_pose_sink`)
  - diccionarios de huesos (`xps_bonedict_sink`)
  - escenas (`xps_scene_sink`)
- lectura ASCII con enteros y decimal/exponente a fixed-point
- lectura binaria little-endian para `.mesh`, `.xps` y `.scene`
- strings compactos XPS (longitud base-128 en 1 o 2 bytes)
- strings de `.scene` con longitud de 1 byte y tolerancia al byte extra `0x01`
- dos pasadas: `scan` y luego `parse`
- límites de compilación para UVs, pesos, nombres, paths y líneas

## Filosofía

- **sin heap**: todo va por stack + callbacks
- **fixed point**: posiciones, normales, UVs, weights, poses y escenas en `xps_fx` (`Q16.16`)
- **dos pasadas**: primero contamos el archivo completo, luego emitimos callbacks de parseo
- **truncación configurable** para UVs/pesos extra

## Build

```sh
make
```

## API pública

```c
int xps_probe_file(const char *path, xps_probe_result *out_probe);

int xps_header_detect_file(const char *path, xps_header_info *out_header);
int xps_header_parse_file(const char *path, const xps_header_sink *sink, void *user);

int xps_detect_file(const char *path, xps_model_info *out_model);
int xps_scan_file(const char *path, xps_scan_result *out_scan);
int xps_parse_file(const char *path, const xps_sink *sink, void *user);

int xps_pose_detect_file(const char *path, xps_pose_info *out_pose);
int xps_pose_scan_file(const char *path, xps_pose_scan_result *out_scan);
int xps_pose_parse_file(const char *path, const xps_pose_sink *sink, void *user);

int xps_bonedict_detect_file(const char *path, xps_bonedict_info *out_dict);
int xps_bonedict_scan_file(const char *path, xps_bonedict_scan_result *out_scan);
int xps_bonedict_parse_file(const char *path, const xps_bonedict_sink *sink, void *user);

int xps_scene_detect_file(const char *path, xps_scene_info *out_scene);
int xps_scene_scan_file(const char *path, xps_scene_scan_result *out_scan);
int xps_scene_parse_file(const char *path, const xps_scene_sink *sink, void *user);
```

## Límites importantes

Edita `include/xps_config.h`:

```c
#define XPS_CFG_MAX_NAME 128
#define XPS_CFG_MAX_TEXNAME 260
#define XPS_CFG_MAX_PATH 260
#define XPS_CFG_MAX_UV_LAYERS 4
#define XPS_CFG_MAX_WEIGHTS 8
#define XPS_CFG_MAX_LINE 1024
```

## Estado actual

- **ASCII modelo**: funcional
- **binario `.mesh` / `.xps`**: funcional para huesos, meshes, texturas, vértices, caras y pesos
- **poses `.pose`**: funcional en texto plano por línea de hueso
- **BoneDict.txt**: funcional como pares `oldName;newName`
- **`.scene`**: funcional para items, pose por item, accesorios, cámara, luces, post, fondo, sky dome y ventana
- **header settings/default pose/flags del `.xps`**: funcional con API separada `xps_header_*`
- **tangentes**: se consumen y descartan

## Notas de implementación

- `xps_probe_file()` separa **modelo**, **pose**, **BoneDict** y **scene**.
- `xps_pose_parse_file()` acepta líneas con 9 números o 6 números + escala implícita `1 1 1`.
- `xps_bonedict_parse_file()` lee una entrada por línea: `oldName;newName`.
- `xps_scene_parse_file()` sigue un flujo de bloques:
  - header/version
  - items
  - cámara
  - 3 luces
  - postprocesado
  - fondo
  - sky dome
  - ventana

Más notas en `docs/FORMAT_NOTES.md`, `docs/API_NOTES.md` y `docs/SCENE_NOTES.md`.

## Header `.xps`

Para archivos `.xps` con header, además del cuerpo del modelo puedes parsear el bloque de metadata:

- `xps_header_detect_file()`
- `xps_header_parse_file()`

El `xps_header_sink` expone:

- `on_header_begin`
- `on_header_option`
- `on_header_flag_pair`
- `on_header_flags`
- `on_header_default_pose_begin`
- `on_header_default_pose_bone`
- `on_header_default_pose_end`
- `on_header_end`

Notas:

- header `legacy_raw`: la librería conserva el tamaño del bloque, pero no intenta interpretarlo.
- header `typed`: decodifica `hash`, `items`, opciones `none`, `default pose` y `flags`.
- el campo `gloss` se entrega como `gloss_raw`, porque la semántica exacta del valor queda ambigua en las referencias de la comunidad.
