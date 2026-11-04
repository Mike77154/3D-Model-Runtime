#include "c4d89.h"
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv)
{
    static C4D89_MeshTable table;
    unsigned int i;int rc;
    if(argc<3 || (strcmp(argv[1],"meshes")!=0 && strcmp(argv[1],"obj")!=0)) {
        fprintf(stderr,"Usage: c4d89_geometry meshes file.c4d | obj file.c4d file.obj\n");return 2;
    }
    if(strcmp(argv[1],"meshes")==0){
        rc=c4d89_find_meshes(argv[2],&table);
        if(rc){fprintf(stderr,"ERROR %d: %s\n",rc,c4d89_error(rc));return rc;}
        printf("Verified native mesh groups: %u\n",table.count);
        for(i=0u;i<table.count;i++){
            const C4D89_MeshDesc*m=&table.meshes[i];
            printf("mesh %u: vertices=%lu tris=%lu quads=%lu uv_corners=%lu; data offsets uv=%lu idx=%lu pos=%lu\n",
                   i,m->vertex_count,m->triangle_count,m->quad_count,m->uv_count,
                   m->uv_offset,m->face_offset,m->vertex_offset);
        }
        return 0;
    }
    if(argc<4){fprintf(stderr,"obj requires output path\n");return 2;}
    rc=c4d89_export_obj(argv[2],argv[3]);
    if(rc)fprintf(stderr,"ERROR %d: %s\n",rc,c4d89_error(rc));
    else printf("Direct C4D decoded scene exported as OBJ preview: %s\n",argv[3]);
    return rc;
}
