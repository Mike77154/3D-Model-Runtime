#include "ss89.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static SS89_Model model;
static SS89_Vertex frame_scratch[SS89_MAX_VERTICES];
int main(int argc, char **argv)
{
    int i, rc;
    unsigned frame=0U, mip=0U;
    const char *out=NULL;
    if(argc<2){
        fprintf(stderr,"usage: ss89dump model.mdl [--mip N] [--frame N] [--obj outfile.obj]\n");
        return 2;
    }
    for(i=2;i<argc;i++){
        if(!strcmp(argv[i],"--mip") && i+1<argc)mip=(unsigned)strtoul(argv[++i],NULL,10);
        else if(!strcmp(argv[i],"--frame") && i+1<argc)frame=(unsigned)strtoul(argv[++i],NULL,10);
        else if(!strcmp(argv[i],"--obj") && i+1<argc)out=argv[++i];
        else {fprintf(stderr,"unknown option: %s\n",argv[i]);return 2;}
    }
    rc=ss89_open(&model,argv[1]);
    if(rc){fprintf(stderr,"error opening: %s at %s\n",ss89_error_string(rc),model.error_at);return 1;}
    if(mip){rc=ss89_select_mip(&model,mip);if(rc){fprintf(stderr,"mip: %s\n",ss89_error_string(rc));ss89_close(&model);return 1;}}
    printf("Serious Engine 1 MDAT V%03u\n",model.version);
    printf("vertices=%u frames=%u mips=%u active_mip=%u triangles=%u UV=%u surfaces=%u\n",
        (unsigned)model.vertices,(unsigned)model.frames,(unsigned)model.mips,
        (unsigned)model.selected_mip,(unsigned)model.triangles_count,
        (unsigned)model.uv_count,(unsigned)model.surface_count);
    printf("tex_MEX=%d x %d collisions=%u attachments=%u animations=%u patches_nonempty=%u\n",
        model.texture_width,model.texture_height,(unsigned)model.collisions,
        (unsigned)model.attachments,(unsigned)model.anim_count,(unsigned)model.patches_present);
    for(i=0;i<(int)model.mips;i++)printf("mip[%d] polygons=%u triangles=%u\n",i,
        (unsigned)model.mip_polygons[i],(unsigned)model.mip_triangles[i]);
    for(i=0;i<(int)model.anim_count;i++) printf("anim[%d] %s keys=%u first_frame=%u\n",i,
        model.anims[i].name,(unsigned)model.anims[i].keys,
        model.anims[i].keys?(unsigned)model.anim_keys[model.anims[i].first_key]:0U);
    if(out){
        rc=ss89_export_obj(&model,frame,out,frame_scratch,SS89_MAX_VERTICES);
        if(rc){fprintf(stderr,"OBJ: %s\n",ss89_error_string(rc));ss89_close(&model);return 1;}
        printf("OBJ exported: %s\n",out);
    }
    ss89_close(&model);
    return 0;
}
