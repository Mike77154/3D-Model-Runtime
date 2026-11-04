# Correspondencias del SDK con runtime C89 (v0.3)

| Concepto Cinema 4D | Evidencia actual | Estado de decoder |
|---|---|---|
| PolygonObject.GetAllPoints | array nativo 0x85 XYZ | SI; Q16.16 |
| CPolygon / indices | array nativo 0x86 | SI; 16-bit indices, triangulos nativos |
| UVWTag / esquina | array nativo 0x85 UVW | SI; UV por esquina con tiling |
| TextureTag / materials | referencias UTF-16BE | nombres SI, asignaciones NO |
| JPEG embebidos | scan JPEG | SI; streams compressed a callbacks |
| NormalTag / Phong | faltan registros verificados | NO |
| GetCache/GetDeformCache | faltan nodos evaluados | NO |
| BaseObject parent/child | nombres y etiquetas | padres/hijos NO |
| CAWeightTag / joints | nombres Weight y Joint | pesos, bindpose, rig NO |
| CTrack/CCurve/CKey | sin secuencias validas | NO |
| PoseMorph/MoGraph | sin estructura validada | NO |

Nota: el SDK sirve como mapa conceptual, NO se enlaza ni se incorpora al
codigo. Los offsets y estructuras de 0x85 y 0x86 se determinaron sobre las
muestras binarias, no copiando serializadores propietarios.
