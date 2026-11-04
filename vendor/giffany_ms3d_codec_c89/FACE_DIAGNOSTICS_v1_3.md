# Diagnóstico de polígonos aparentemente ausentes — MS3D v1.3

## Pruebas sobre los archivos binarios originales

| Modelo | Vértices | Triángulos | Aristas de borde usando índices | Bordes tras agrupar posiciones idénticas |
|---|---:|---:|---:|---:|
| zomb.ms3d | 765 | 1505 | 19 | 19 |
| kamikaze.ms3d | 765 | 1505 | 19 | 19 |
| zmb.ms3d | 657 | 735 | 511 | 31 |
| zmb_hs.ms3d | 630 | 685 | 505 | 71 |

Los primeros dos mantienen el mismo número original de triángulos; no
se omitieron caras en la exportación. Los dos `zmb*` poseen cientos de
bordes abiertos **por índice**, pero la mayoría se solapan geométricamente:
son vértices duplicados (costuras) y no caras eliminadas.

Los 31/71 bordes geométricos restantes pueden ser huecos auténticos,
terminaciones intencionales de ropa/extremidades, o piezas que el juego
ensambla por separado. **Este diagnóstico no demuestra cuál de esos
motivos aplica a cada borde.** Se necesitaría inspección topológica
localizada y materiales originales para decidir qué reparar.

De modo independiente, backface culling, transparencia y un sombreado
muy oscuro pueden producir *falsos* huecos; las vistas software
anteriores renderizaban ambos lados de los triángulos.

### Cambios integrados

1. El exportador animado emite **exactamente una vez** cada triángulo MS3D
   directamente por índice, sin depender de que figure en un grupo.
2. Smooth shading por esquina con normales autorales o reconstrucción.
3. Nuevo `--smooth-weld`: promedia normales en vértices duplicados con
   posiciones idénticas, siempre que coincida su grupo de suavizado.
4. Ningún triángulo original se elimina o modifica automáticamente.
5. `tools/audit_faces.py` permite repetir la auditoría de bordes sobre otros
   MS3D sin depender del renderer.

### Aviso sobre backface culling

Si un modelo se ve agujereado **sólo desde una dirección**, primero probar
con culling deshabilitado y dibujado de ambas caras. Si persiste, revisar
el conteo de caras exportadas y los bordes geométricos. No invertir
winding globalmente sin verificar materiales y normales.

### Fuentes de la especificación

- https://sappersblog.blogspot.com/2014/08/milkshape-3d-185-ms3d-file-format.html
- https://paulbourke.org/dataformats/ms3d/index.html

Las mediciones se hicieron directamente del binario MS3D v4, no a partir
de una imagen ni de OBJ convertido.
