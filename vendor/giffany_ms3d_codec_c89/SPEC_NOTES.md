# MS3D spec notes usadas para el codec

## Magic/version

El archivo binario inicia con:

```text
id[10]  = "MS3D000000"
version = 4
```

El codec rechaza otros magic/version para no adivinar variantes.

## Orden de secciones

El flujo base usado por el parser:

```text
header
u16 vertex_count
vertices[vertex_count]
u16 triangle_count
triangles[triangle_count]
u16 group_count
groups[group_count]
u16 material_count
materials[material_count]
anim_fps
current_time
total_frames
u16 joint_count
joints[joint_count]
optional comments
optional vertex extra
optional joint extra
optional model extra
```

## Tamaños base esperados del formato

```text
vertex   = 15 bytes
triangle = 70 bytes
material = 361 bytes
group    = dynamic: flags + name[32] + count + triangle indices + material index
joint    = dynamic: base + rot key list + pos key list
```

## Cosas que el codec conserva

- Vertex flags.
- Position Q16.16.
- Bone id base.
- Triangle indices.
- Normals Q16.16.
- UV `s/t` Q16.16.
- Smoothing group.
- Group index.
- Groups con índice inicial a una tabla plana de triangle indices.
- Material colors, shininess, transparency Q16.16.
- Texture/alphamap paths.
- Animation FPS/current time/total frames.
- Joints base, rot keys, pos keys.
- Parent index resuelto por nombre.
- Comments de group/material/joint/model en buffers fijos.
- Vertex extras subversion 1/2/3.
- Influencias normalizadas a Q16.16 por helper.
- Joint colors.
- Model extra: joint size, transparency mode, alpha ref.

## Restricciones intencionales

- No genera matrices finales ni skinning final en este paquete; eso queda para el renderer/animator del engine.
- No usa heap para comments; los comentarios se guardan en buffers fijos y se truncarán si exceden `MS3D_MAX_COMMENT_CHARS` o `MS3D_MAX_MODEL_COMMENT_CHARS`.
- El sampling de animación es local y lineal. No intenta resolver Euler a matriz ni hacer blending global.
- Los campos continuos extremos se saturan a rango Q16.16.


## Notas posteriores a la regresión (v1.1)

**Comentarios opcionales.** Un bloque `comments_subversion=1` con los cuatro contadores vacíos es válido y ocupa 20 bytes. Nunca retroceder sólo porque las cuatro listas estén vacías: la siguiente subversión suele ser `vertex_extra=2`.

**Pesos.** La asignación de `weights[0..2]` es (boneId, boneIds[0], boneIds[1]); `boneIds[2]` recibe el residual. La suma supera 100/255 en assets mal formados: se normaliza a las contribuciones explícitas sin residual en ese caso.

**No confundir sampling y rigging final.** Los arrays de animación y el parent_index se conservan; la aplicación de matrices globales y skinning sigue siendo responsabilidad del host/animator.


## Hallazgos de investigación v1.2

- La especificación MS3D 1.8.5 define `triangle.vertexNormals[3][3]` con
  **tres normales por cara**, `s[3]`, `t[3]`, `smoothingGroup` (1..32) y
  `groupIndex`. El `smoothingGroup` se almacena en **un byte como identificador
  de grupo**, no como bitmask de múltiples grupos.
- En el corpus Night of the Zombies, los 157 assets usan smoothing IDs 1..3,
  con 99,423/8,548/1,780 triángulos por cada uno. No había grupo 0.
- Los archivos `zomb.ms3d` y `kamikaze.ms3d` ya incluyen normales autorales
  suavizadas; la vista anterior que coloreaba por triángulo estaba mostrando
  color plano, no una lectura incorrecta del formato.
- No confundir normales por esquina con normales por vértice: en una arista
  dura, un mismo vértice necesita distintas normales en sus esquinas.
- El re-cálculo nuevo hace un promedio unitario de normales geométricas de
  caras adyacentes que comparten **índice** y smoothing ID. No fusiona vértices
  duplicados de seams UV. Tampoco implementa ponderación angular.
- En un renderer, activar interpolación de normales (Gouraud/Phong) es
  imprescindible; `GL_SMOOTH` no reconstruye ninguna normal por sí mismo.
- El corpus adicional `dwarf.ms3d` requiere 7074 rotation/position keys,
  y motivó aumentar el límite fijo a 8192.

Referencias públicas consultadas:
- https://sappersblog.blogspot.com/2014/08/milkshape-3d-185-ms3d-file-format.html
- https://paulbourke.org/dataformats/ms3d/index.html
- https://archive.blender.org/wiki/2015/index.php/Extensions:2.6/Py/Scripts/Import-Export/MilkShape3D_MS3D/
- https://help.autodesk.com/cloudhelp/2023/ENU/3DSMax-Modeling/files/GUID-1DB2E3C2-CE68-4AF7-899E-01B90F7EB320.htm
