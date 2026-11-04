#include "ifc3d_internal.h"
#include <stdio.h>

ifc3d_status ifc3d_write_obj(const ifc3d_mesh *mesh, const char *path, ifc3d_diag *diag)
{
    FILE *f;
    int i;
    char sx[32], sy[32], sz[32];
    if (!mesh || !path) return IFC3D_ERR_BAD_ARG;
    f = fopen(path, "wb");
    if (!f) {
        ifc3d_set_diag(diag, IFC3D_ERR_PARSE, 0, "could not open OBJ output");
        return IFC3D_ERR_PARSE;
    }
    fprintf(f, "# ifc3d_c89 generated OBJ\n");
    fprintf(f, "# vertices %d triangles %d\n", mesh->vertex_count, mesh->tri_count);
    for (i = 0; i < mesh->vertex_count; ++i) {
        ifc3d_fixed_to_cstr(mesh->vertices[i].x, sx, sizeof(sx), 6);
        ifc3d_fixed_to_cstr(mesh->vertices[i].y, sy, sizeof(sy), 6);
        ifc3d_fixed_to_cstr(mesh->vertices[i].z, sz, sizeof(sz), 6);
        fprintf(f, "v %s %s %s\n", sx, sy, sz);
    }
    for (i = 0; i < mesh->tri_count; ++i) {
        fprintf(f, "f %d %d %d\n", mesh->tris[i].a + 1, mesh->tris[i].b + 1, mesh->tris[i].c + 1);
    }
    fclose(f);
    ifc3d_set_diag(diag, IFC3D_OK, 0, "ok");
    return IFC3D_OK;
}
