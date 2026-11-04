# Corpus de documentación recibida — revisión técnica, 2026-10-10

Todos los repositorios fueron leídos como **referencia**, sin venderizar ni
copiar sus implementaciones en la biblioteca C89. Las licencias varían.

| Archivo recibido | Lecciones útiles | Decisión |
|---|---|---|
| `ms3d.c` | Loader compacto C; demuestra offsets, structs packed, triángulos con normales por esquina y grupos dinámicos. Usa `stdint`, float, malloc y acceso a structs binarios. | No copiar: distinto de C89/Q16/sin heap; útil como comparación. |
| `imbalance-master.zip` (`libms3d`) | Matrices jerárquicas para joints, postmultiplicación de transformaciones, interpolación de claves, normales sometidas a inverse bind, y texturas OpenGL. Código comentado muestra propuesta de cuaterniones. | Referencia para futuro skinning; en `libms3d/doc/LICENSE.TXT` consta una licencia Creative Commons BY-NC-ND; no vendorizar. |
| `UnityExtraAnimations-master.zip` | Importación de modelos/animaciones Unity y herramienta para dividir clips; `dwarf.ms3d` aporta 46 joints y 7,074 claves de cada tipo. | Se incorporó su modelo sólo a las pruebas locales; no el código Unity. |
| `milkshape_plugins-master.zip` | Plugin SDK Delphi/Object Pascal: modelo mutable desde MilkShape, mesh topology/adyacencia; licencia MIT. | No necesitamos API de plugin para leer MS3D, pero servirá al editor. |
| `ms3dloader-master.zip` | Ejemplo C++/Win32/OpenGL: `glShadeModel(GL_SMOOTH)`, normales por esquina emitidas antes de vértices, JPEG texture lookup. Licencia Apache-2.0. | Confirma la importancia del pipeline de render; no incorporar dependencias Win32/JPEG. |

## Notas para el siguiente hito técnico

1. Skinning: matrices de bind pose/inverse bind, acumulación padre-hijo y
   transformación normal separada de traslación.
2. Rotaciones: interpolar Euler linealmente puede dar saltos de 2π e
   interpolación imperfecta; experimentar con quaterniones fijos con pruebas.
3. Render de materiales: difuso/emisivo/especular, transparencia y `alphamap`;
   `transparency_mode` determina política de mezcla/ordenación.
4. Dibujado: el códec guarda normales originales; reconstruir cuando las
   normales del modelo estén dañadas o cuando se desee override creativo.
5. La triangulación es fija: suavizado no genera malla de mayor densidad.

## Limitaciones del experimento

- `ms3d_emit_shaded_corners` y export OBJ son de **pose base**.
- Preview es rasterizador de validación de iluminación Gouraud, sin texturas
  originales ni animación.
- El promedio de normales por grupo es por **índice de vértice**; seam UV que
  duplica vértices puede conservar una unión perceptible, como corresponde
  a un importador sin welding geométrico.
- Un objeto arquitectónico debe usar `--smooth` o `--source`, no `--smooth-all`.
- Ni `ms3d.c` ni los repositorios de referencia se redistribuyen en el ZIP
  fuente de nuestra biblioteca.
