# Giffany MS3D Codec v1.2 — investigación y smooth shading

**Fecha:** 2026-10-10  
**Implementación:** C89, Q16.16 fixed-point, sin malloc/free/realloc, sin floats o doubles en el códec; objetos persistentes estáticos.  
**Objetivo:** interpretar normales MS3D por esquina y conservar los grupos de suavizado con exportación portable a motores y OBJ.

## Resultado verificable

- 157 archivos `.ms3d` de Night of the Zombies recuperados de la prueba anterior.
- 3 casos externos adicionales recibidos: `Model.ms3d` (imbalance), `character.ms3d` (ms3dloader), `dwarf.ms3d` (UnityExtraAnimations).
- **160/160** pasaron parseo y exportación OBJ con reconstrucción de normales `--smooth` sin errores.
- Ampliado el límite de rotation keys y position keys de 6144 a 8192: `dwarf.ms3d` usa 7074 + 7074 keys.
- Nuevo `gif_ms3d_normals.c/h`, con cuatro modos: normales originales, smooth con smoothing groups, smooth-all y flat.
- Nuevo `ms3d_emit_shaded_corners` para alimentar a los motores sin intermediario: por triángulo y esquina proporciona posición, normales, UV, grupo e índice de material.
- Los tests C89 bajo `-std=c89 -O2 -Wall -Wextra -Werror -pedantic` pasan; test sintético verifica grupos 0, 1, 2 y esquinas compartidas.
- Preview comparativo Gouraud en software rasterizer usando únicamente las normales exportadas por el códec C89. Pose base, **sin texturas originales**.

## Distribución de suavizado del corpus

| Smoothing ID | Triángulos |
|---|---:|
| 1 | 99,423 |
| 2 | 8,548 |
| 3 | 1,780 |
| **Total** | **109,751** |

Los autores ya introdujeron grupos diferentes en vehículos y estructuras. Los zombis `zomb.ms3d` y `kamikaze.ms3d` tienen todas sus caras en grupo 1. Las normales autorales de `zomb` son prácticamente idénticas a las reconstruidas: el aspecto facetado de la previa anterior provenía sobre todo de la iluminación/color plano por triángulo.

## Recomendación de uso

- Para fiel reproducción: conservar las normales originales (`--source`) y hacer interpolación real por esquina en el renderer.
- Para modelos con normales incorrectas/ausentes: `--smooth`, reconstrucción por índice y grupo (1..32), grupo 0 queda plano.
- Para retoque creativo: `--smooth-all` en mallas orgánicas; evitarlo en edificios y cajas.
- Para diagnóstico: `--flat` muestra triangulación/facetas.

**No se modifican los triángulos, los UV o la pose base.** Smooth shading no crea más vértices geométricos ni subdivision surfaces. OBJ no conserva joints, pesos ni keyframes; eso permanece dentro del modelo `ms3d_model` que el runtime puede consumir.

## Evaluación de archivos entregados

| Archivo | Aprendizaje concreto | Incorporado |
|---|---|---|
| `ms3d.c` | Layout del archivo y normales por esquina; ejemplo simple usa malloc/float y packed structs. | Sólo investigación. |
| `imbalance-master.zip` | Animación matricial y relaciones padre-hijo; normal transform en bind pose; fuente de ejemplo `Model.ms3d`. | Modelo como regresión; pendiente matrices. |
| `UnityExtraAnimations-master.zip` | Parser Unity/MS3D y división de clips; `dwarf.ms3d` con 7074 rot/pos keys. | Capacidad de keys ampliada; regresión nueva. |
| `milkshape_plugins-master.zip` | Interfaz de plugins Delphi/Object Pascal y operaciones topológicas. | Referencia para futuro editor. |
| `ms3dloader-master.zip` | Ejemplo Win32 OpenGL usando `glShadeModel(GL_SMOOTH)` y `glNormal` por esquina. | Integración mediante sink portable, sin portar OpenGL al códec. |

Los paquetes externos tienen distintas licencias. No se copiaron sus implementaciones dentro de nuestro código.

## Qué falta

1. Skinning final: matrices bind/inverse bind jerárquicas y transformación de normales en pose animada.
2. Interpolación avanzada de rotaciones: quaterniones fixed-point y control de ángulos Euler/rango ±π.
3. Materialización completa: texturas y alphamap, shininess, translucidez/transparency_mode y mapas adicionales si el formato de origen los aporta.
4. Weld opcional de vértices duplicados en seams UV para suavizar superficies que se discontinuaron durante la exportación, con respeto a materiales y aristas duras.
5. Pruebas de reproducción de múltiples clips y animaciones en visor, todavía no realizadas.

## Fuentes de documentación

- [MilkShape 3D 1.8.5 — especificación corregida por Sapper](https://sappersblog.blogspot.com/2014/08/milkshape-3d-185-ms3d-file-format.html)
- [Paul Bourke — MilkShape 3D binary format](https://paulbourke.org/dataformats/ms3d/index.html)
- [Blender MS3D importer/exporter y smoothing groups](https://archive.blender.org/wiki/2015/index.php/Extensions:2.6/Py/Scripts/Import-Export/MilkShape3D_MS3D/)
- [Autodesk — viewing and changing smoothing](https://help.autodesk.com/cloudhelp/2023/ENU/3DSMax-Modeling/files/GUID-1DB2E3C2-CE68-4AF7-899E-01B90F7EB320.htm)

## Reproducir la prueba

```sh
make check
./examples/ms3d_obj_export zomb.ms3d zomb_smooth.obj --smooth
./examples/ms3d_obj_export zomb.ms3d zomb_flat.obj --flat
./examples/ms3d_obj_export auto.ms3d auto_smooth.obj --smooth
```
