/* Strict C89 offline metadata scan: no heap, no floating-point. */
#include "hl2modelib.h"
#include <stdio.h>
#include <string.h>
#define HOD2_BYTES_CAP (8u*1024u*1024u)
static unsigned char g_bytes[HOD2_BYTES_CAP];
static hl2m_mdl g_model;
int main(int argc,char **argv)
{
    FILE *fp;
    size_t got;
    unsigned int size;
    int fixed,r,i,sectioned,with_motion;
    if(argc!=2){fprintf(stderr,"usage: hl2m_hod2_model_probe113 file.mdl\n");return 1;}
    fp=fopen(argv[1],"rb");
    if(!fp){perror(argv[1]);return 2;}
    got=fread(g_bytes,1,HOD2_BYTES_CAP,fp);
    if(ferror(fp)||!feof(fp)){fclose(fp);return 3;}
    fclose(fp);
    size=(unsigned int)got;
    fixed=0;
    r=hl2m_repair_missing_first_byte(g_bytes,&size,HOD2_BYTES_CAP,HL2M_FORMAT_MDL,&fixed);
    if(r!=HL2M_OK){printf("header_error=%d\n",r);return 4;}
    r=hl2m_decode_mdl(g_bytes,size,&g_model);
    if(r!=HL2M_OK){printf("mdl_error=%d\n",r);return 5;}
    sectioned=0;with_motion=0;
    for(i=0;i<g_model.anim_count;++i){
        if(g_model.anims[i].sectionframes>0)++sectioned;
        if(g_model.anims[i].nummovements>0)++with_motion;
    }
    printf("MODEL %s version=%d bones=%d anim=%d seq=%d sections=%d motions=%d textures=%d meshes=%d bodyparts=%d flex=%d\n",g_model.name,g_model.version,g_model.bone_count,g_model.anim_count,g_model.sequence_count,sectioned,with_motion,g_model.texture_count,g_model.mesh_count,g_model.bodypart_count,g_model.flexdesc_count);
    for(i=0;i<g_model.anim_count;++i){
        printf("ANIM %d name=%s frames=%d sectionframes=%d block=%d\n",i,g_model.anims[i].name,g_model.anims[i].numframes,g_model.anims[i].sectionframes,g_model.anims[i].animblock);
    }
    return 0;
}
