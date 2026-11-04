#include "../gs3d_internal.h"
/* Exact named frame lookup; no duplicated animation frame tables. */
int gs3d_find_frame(unsigned int group,const char *name){
    unsigned int i;GS3D_Group *g;
    if(!name||!gs_is_open||group>=gs3d_model.groups)return -1;
    g=&gs3d_model.group[group];
    for(i=0U;i<g->frame_count;i++)
        if(!strcmp(g->frames[i].name,name))return (int)i;
    return -1;
}
