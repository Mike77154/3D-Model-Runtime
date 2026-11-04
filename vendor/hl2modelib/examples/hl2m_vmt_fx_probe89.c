/* Source VMT material inspection, standard C89, statically allocated buffers. */
#include "hl2modelib.h"
#include <stdio.h>
static hl2m_u8 source[65536];
static hl2m_vmt vmt;
static hl2m_material_fx89 fx;
int main(int argc, char **argv)
{
    FILE *file; size_t n; int fixed=0,r; unsigned int size;
    if(argc!=2){fputs("usage: hl2m_vmt_fx_probe89 material.vmt\n",stderr);return 1;}
    file=fopen(argv[1],"rb");if(!file){perror(argv[1]);return 2;}
    n=fread(source,1u,sizeof(source)-1u,file);fclose(file);
    size=(unsigned int)n;
    r=hl2m_repair_missing_first_byte(source,&size,sizeof(source),HL2M_FORMAT_VMT,&fixed);
    if(r){fprintf(stderr,"header repair %d\n",r);return 3;}
    r=hl2m_decode_vmt(source,size,&vmt);
    if(r){fprintf(stderr,"VMT parse error %d\n",r);return 4;}
    r=hl2m_vmt_to_fx89(&vmt,&fx);if(r)return 5;
    printf("VMT parsed repaired=%d base=%s bump=%s detail=%s env=%s\n",fixed,fx.material.base_texture,fx.material.bumpmap,fx.material.detail,fx.material.envmap);
    printf("fx phong=%d rim=%d normal=%d detail=%d env=%d alpha-test=%d no-cull=%d\n",
           fx.phong,fx.rimlight,fx.normal_map,fx.detail_map,fx.environment,
           (fx.material.flags&HL2M_VMT_FLAG_ALPHATEST)!=0,
           (fx.material.flags&HL2M_VMT_FLAG_NOCULL)!=0);
    return 0;
}
