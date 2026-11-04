#include "../../include/gs3d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv){
    int r;unsigned int g,j;unsigned long w=0UL,h=0UL;
    if(argc<2){puts("usage: gs3d_probe model.mdl [frame_number] [skin_number] [pose]");return 2;}
    for(j=0U;j<256U;j++){
        gs3d_palette[j*3U]=(unsigned char)j;
        gs3d_palette[j*3U+1U]=(unsigned char)j;
        gs3d_palette[j*3U+2U]=(unsigned char)j;
    }
    r=gs3d_open(argv[1]);
    if(r){printf("ERROR %d %s\n",r,gs3d_error_name(r));return 1;}
    printf("OK MDL%d | groups=%u bones=%u verts=%lu uv=%lu faces=%lu\n",
        gs3d_model.format,gs3d_model.groups,gs3d_model.bones,
        gs3d_model.vertices,gs3d_model.uvs,gs3d_model.faces);
    for(g=0U;g<gs3d_model.groups;g++){
        GS3D_Group *m=&gs3d_model.group[g];
        printf("group %u '%s': verts=%lu tris=%lu uv=%lu skins=%u frames=%u\n",
            g,m->name,m->vertex_count,m->face_count,m->uv_count,m->skin_count,m->frame_count);
        if(argc>2 && m->frame_count){
            j=(unsigned int)atoi(argv[2]);
            r=(argc>4 && !strcmp(argv[4],"pose")) ? gs3d_apply_pose(g,j) : gs3d_apply_frame(g,j);
            printf("  frame %u: %s\n",j,gs3d_error_name(r));
        }
        if(argc>3 && m->skin_count){
            j=(unsigned int)atoi(argv[3]);r=gs3d_decode_skin(g,j,&w,&h);
            printf("  skin %u: %s (%lux%lu)\n",j,gs3d_error_name(r),w,h);
        }
    }
    gs3d_close();return 0;
}
