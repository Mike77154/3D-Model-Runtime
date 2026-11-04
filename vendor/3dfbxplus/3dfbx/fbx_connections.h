#ifndef FBX_CONNECTIONS_H
#define FBX_CONNECTIONS_H

#include "fbx_importer.h" /* FbxNode */

#ifdef __cplusplus
extern "C" {
#endif

struct FbxConnectionsTag;

/* Tipo de relación:
   - "OO" = Object → Object
   - "OP" = Object → Property
   Guardamos la cadena y, opcionalmente, el nombre de propiedad.
*/
typedef struct FbxConnectionTag {
    struct FbxConnectionsTag *owner; /* puntero al contenedor (para iteración) */

    fbx_i64 from;               /* ID hijo   (child)  */
    fbx_i64 to;                 /* ID padre  (parent) */
    char    rel[3];             /* "OO", "OP", etc. (2 chars + '\0') */
    char   *prop_name;          /* sólo si rel == "OP", puede ser NULL */

    /* Índices internos (si se construyen arrays ordenados).
       Sirven para que find_next_* sea correcto incluso cuando
       find_first_* se apoyó en índices. */
    int     idx_by_from;
    int     idx_by_to;

    struct FbxConnectionTag *next;
} FbxConnection;

/* Contenedor de conexiones */
typedef struct FbxConnectionsTag {
    FbxConnection  *head;     /* lista enlazada simple de conexiones */
    int             count;    /* nº de conexiones */

    /* índices opcionales (se construyen en build) */
    FbxConnection **by_from;  /* array sorted por .from */
    FbxConnection **by_to;    /* array sorted por .to */
} FbxConnections;

/* Construir la lista de conexiones leyendo el nodo "Connections" del árbol FBX.
   - root: nodo raíz del archivo (el que devuelve fbx_read_scene).
   Devuelve NULL en error; si no hay nodo Connections devuelve una estructura
   válida pero vacía (head == NULL, count == 0).
*/
FbxConnections *fbx_connections_build(FbxNode *root);

/* Liberar todas las conexiones + índices */
void fbx_connections_destroy(FbxConnections *conns);

/* Helpers de búsqueda */

FbxConnection *fbx_connections_find_first_to(FbxConnections *conns,
                                             fbx_i64 to,
                                             const char *rel_filter);

FbxConnection *fbx_connections_find_next_to(FbxConnections *conns,
                                            const FbxConnection *prev,
                                            fbx_i64 to,
                                            const char *rel_filter);

FbxConnection *fbx_connections_find_first_from(FbxConnections *conns,
                                               fbx_i64 from,
                                               const char *rel_filter);

FbxConnection *fbx_connections_find_next_from(FbxConnections *conns,
                                              const FbxConnection *prev,
                                              fbx_i64 from,
                                              const char *rel_filter);

/* Helper rápido: devuelve el primer "padre" de un objeto
   (primer connection from=child_id, rel_filter), o 0 si no encuentra ninguno. */
fbx_i64 fbx_connections_get_parent(FbxConnections *conns,
                                   fbx_i64 child_id,
                                   const char *rel_filter);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FBX_CONNECTIONS_H */
