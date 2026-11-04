# Modelo Runtime Experimental — primera tanda de 10 (Wave 5)

## Resumen verificable

- **45 proveedores registrados:** 35 previamente presentes y 10 wrappers nativos (IDs 61–70).
- Los nuevos wrappers llaman directamente a la implementación C89 vendorizada. **No usan GACKT-LANG, `gackt_passthrough_open_profile` ni `.gmesh`/`.gscene`.**
- Se compiló toda la biblioteca con `-std=c89 -pedantic-errors -Wall -Wextra -O2` en GCC; `AMF_STRICT_C89_NO_LONGLONG=1` activa la alternativa C89 propia del vendor AMF.
- Registro comprobado en la API pública, con 45 proveedores.
- El ZIP contiene solamente fuentes, headers, scripts y documentación; no corpus, imágenes ni objetos/ejecutables compilados.

## Diez wrappers incluidos

| ID | Códec | Función principal | Estado de corpus |
|---:|---|---|---|
| 61 | ACC | `acc3d_decode_memory`, `acc3d_triangulate` | `Wuson.acc` rechazado; variante de vértices extendidos pendiente |
| 62 | SIB | `sib_decode_memory` | `heffalump.sib`: 676 triángulos |
| 63 | COB | `cob_decode_memory`, `cob_scene_build_triangles` | ASCII/binario molecule: 896 triángulos; otro ASCII falla |
| 64 | MD5Mesh | `md5fx_mesh_load_file`, `md5fx_mesh_bind_pose`, `md5fx_mesh_build_vertices` | `SimpleCube.md5mesh`: 12 triángulos; `OverflowingTriangleIndex` rechazado |
| 65 | AMF XML | `amf_parse_xml` | `test1/2/3.amf`: 4 triángulos cada uno |
| 66 | Terragen | `ter3d_read_info`, `ter3d_terragen_get_height_q16` | `RealisticTerrain.ter`: 131072 triángulos |
| 67 | NDO | `ndo3d_decode`, `ndo3d_face_vertices` | archivo corrupto rechazado; faltan muestras válidas |
| 68 | ASK | `ask3d_parse_memory` | sin muestras externas |
| 69 | DBO | `dbo_parse_memory`, `dbo_fvf_decode_position` | sin muestras externas; subconjunto de primitivas |
| 70 | AMJ | `amj3d_decode` con callbacks | sin muestras externas; binario propietario no reconstruido |

## Límites y advertencias

- **Registrar diez proveedores no significa certificar diez formatos.** Cinco formatos tienen al menos una muestra externa que devuelve triángulos; cuatro carecen de corpus positivo y ACC presenta un rechazo de compatibilidad.
- `modelrt_visit()` hace triangulación/visita básica. No hemos certificado fidelidad de texturas, jerarquía, animaciones, transformaciones complejas ni archivos arbitrarios.
- El ID 64 cubre **MD5Mesh estático en bind pose**, no el flujo completo MD5Anim. Se validan índices de vértices por triángulo y se rechazan índices textuales desbordados.
- AMF se limita actualmente a XML sin compresión ZIP. TER al formato Terragen, no a todas las variantes SWBF del vendor.
- NDO, ASK, DBO y AMJ deben considerarse **adaptadores experimentales sin validación positiva de corpus**.
- No utilizar los decodificadores experimentales con archivos no confiables sin fuzzing y auditorías adicionales.
- Los 14 proveedores iniciales no fueron modificados en este lote, pero falta una regresión completa entre plataformas.

## Reproducir

```
make clean
make -j6
make native-batch5-build
./tests/native_batch5_regression +/ruta/RealisticTerrain.ter +/ruta/SimpleCube.md5mesh -/ruta/OverflowingTriangleIndex.md5mesh
```

El prefijo `+` exige abrir y visitar al menos un triángulo. `-` exige rechazo. Sin prefijo solo se reporta diagnóstico.

Los archivos usados se encuentran en los corpus públicos proporcionados en la conversación y **no se incluyen en este ZIP**.

Archivos de integración: `src/providers/provider_native_batch5a_89.c`, `src/providers/provider_native_batch5b_89.c` y `include/modelrt_native_batch5_89.h`.

**Siguiente lote propuesto:** formatos con corpus externo y ruta nativa clara; evitar duplicates del runtime existente, y resolver después IQM y M3D que requieren revisión C89/comprensión.
