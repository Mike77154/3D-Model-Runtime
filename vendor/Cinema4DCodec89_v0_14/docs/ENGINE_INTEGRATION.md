# Integracion directa con game engines C89

La biblioteca ahora DECODIFICA vertices/indices/UV de `.c4d` nativos.
No necesita `.c4b89` ni exportar OBJ. Este ejemplo deliberadamente no
almacena la geometria; un motor puede enviar los datos al subsistema grafico
o a sus arreglos fijos de vertices e indices.

```c
#include "c4d89.h"
#include <string.h>
#include <stdio.h>

static int mesh_begin(void *ctx,unsigned int index,const C4D89_MeshDesc *d)
{
    (void)ctx;
    printf("mesh %u: %lu vertices / %lu caras\n",index,
           d->vertex_count,d->face_count);
    return 1;
}
static int mesh_vertex(void *ctx,unsigned long i,long x,long y,long z)
{
    (void)ctx;(void)i;
    /* XYZ son Q16.16: x/65536, y/65536, z/65536, SIN float. */
    (void)x;(void)y;(void)z;
    return 1;
}
static int mesh_uv(void *ctx,unsigned long i,long u,long v)
{
    (void)ctx;(void)i;(void)u;(void)v;
    /* 4 UV por poligono, incl. duplicado para los triangulos. */
    return 1;
}
static int mesh_face(void *ctx,unsigned long i,const C4D89_Face *face)
{
    (void)ctx;(void)i;
    /* Indices 0-based. face->corners == 3 o 4. */
    (void)face;
    return 1;
}
static int mesh_end(void *ctx,unsigned int index)
{ (void)ctx;(void)index;return 1; }

int load_c4d_mesh(const char *path)
{
    C4D89_MeshReceiver r;
    memset(&r,0,sizeof(r));
    r.sink.begin=mesh_begin;
    r.sink.vertex=mesh_vertex;
    r.sink.uv=mesh_uv;
    r.sink.face=mesh_face;
    r.sink.end=mesh_end;
    r.user=0;
    return c4d89_load_mesh(path,&r);
}
```

El programa exportador OBJ de `src/c4d89_mesh_obj.c` es OPCIONAL: se
usa unicamente como oraculo visual de depuracion para el desarrollo del decoder.
Las rutas `c4d89_stream_scene` y `c4d89_load_mesh` funcionan sin el exportador.

Consideraciones de motor:

1. Aplicar conversion de coordenadas/orientacion explicitamente en la capa
   de runtime; los XYZ se entregan como figuran en el bloque binario C4D.
2. Reconstruir normales de cara para render provisional. Suavizado por
   grupos Phong aun no esta disponible.
3. Los UV se proporcionan por esquina para conservar seams, no por vertice
   compartido. La UV cuarta de los triangulos repite la tercera.
4. Son mallas locales de geometria: la transformacion global y el parent-child
   del scene graph aun no se recuperan. NO inventar un rig.
5. Separar recursos JPEG embebidos, nombres de texturas externas y vinculos
   de material; por ahora solo se leen los dos primeros.
6. Establecer limites de streaming y tratar `C4D89_ERR_LIMIT`/
   `C4D89_ERR_CORRUPT` como fallos duros, sin geometria substituta.
