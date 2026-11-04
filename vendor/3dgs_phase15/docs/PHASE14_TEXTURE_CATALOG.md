# Fase 14: catálogo de skins y poses MDL3/4/5/7

## Objetivo

Enumerar cada skin nativa (incluidas las no seleccionadas por las caras), identificar los tipos de almacenamiento y decodificar RGBA para VTK en memoria. El frontend no crea OBJ, TGA ni GLB para renderizar.

## Nuevos componentes

- `gs3d_texture_info(group,slot,out)`: tipo original, tamaño del blob, alias, referencia, ruta externa, material.
- `gs3d_texture_resolve`: atraviesa referencias del mismo grupo, valida índices y ciclos.
- `gs3d_decode_skin`: decodifica RGB565, ARGB4444, RGB888, RGBA8888, paleta y mipmaps existentes; añade DDS DXT1/DXT3/DXT5 y 32-bit por máscaras.
- `gs3d_set_external_texture_decoder`: callback opcional del host para cargar imágenes externas a RGBA de forma controlada (no garantiza que el archivo exista); no se redistribuye un decodificador PNG/JPEG.
- `gs3d_vtk_skin_info`: consulta directa sin decodificar.
- `tools/vtk_skin_catalog.py`: lámina PNG VTK de **todas** las skins y catálogo JSON con todos los nombres de fotograma.
- `tools/vtk_capture.py --skin N`: permite previsualizar una skin alternativa aplicada a la malla, **solo para diagnóstico**, aunque no sea la textura por material que eligió MED.

## Verificación auténtica del corpus subido

| Modelo | Skins decodificadas | Frames registrados | Validación esquelética |
|---|---:|---:|---|
| Goddess4arms | 1/1 | 37 | Experimental |
| LadyJ | 1/1 | 1 | Sin animación reproducible |
| NinjaGirlShinobi1 | 1/1 | 120 | Experimental |
| ShinobiJ | 1/1 | 1 | Sin animación reproducible |
| Tusker | 4/4 | 139 | Experimental (deformaciones pendientes) |
| TuskerSpear | 4/4 | 139 | Experimental |
| **Total** | **12/12** | **437** | |

Los valores anteriores son lecturas del corpus real (no tests sintéticos). Todas las skins observadas aquí son embebidas y decodificables; la decodificación DDS, referencias y resolución externa se verificó **solo en fixtures sintéticos**.

## Notas de interpretación

- Presencia de una skin != asignación al material de una cara. Tusker declara cuatro skins, aunque las caras usen índice 0; no sustituir automáticamente las otras tres en la malla.
- Una skin de tonos rosados puede ser un normal map, pero no se certifica el canal por color aparente.
- `candidate_ranges_by_prefix` agrupa nombres similares; es una heurística de catálogo y no metadatos de FPS ni clip oficiales.
- Las poses MDL7 pueden estar deformadas por las convenciones aún no verificadas de matrices/espacio de huesos. Las capturas de fotogramas no son equivalentes a reproducción certificada de Acknex.
- Sin capacidad de cargar el fichero externo ni callback, la skin externa se marca como pendiente y nunca se rellena con una textura falsa.

## Compilar y probar

```sh
sh scripts/build_vtk_bridge.sh
sh scripts/check_phase14.sh
python3 tools/vtk_skin_catalog.py /carpeta/de/modelos /carpeta/de/capturas
python3 tools/vtk_capture.py /carpeta/Tusker.mdl captura_tusker_skin2.png --skin 2
```

El núcleo C89 no usa malloc/calloc/realloc/free, float/double ni long long. La interfaz VTK es un programa Python separado y usa los tipos numéricos internos de VTK.
