#include "hl2modelib.h"
#include <stdio.h>
static hl2m_u8 buf[8u*1024u*1024u];
static hl2m_mdl mdl;
int main(int argc,char **argv)
{
    FILE *fp;unsigned int n;int fixed=0;int i,r;
    if(argc!=2){fputs("usage: hl2m_anim_sdk_audit89 model.mdl\n",stderr);return 1;}
    fp=fopen(argv[1],"rb");if(!fp)return 2;
    n=(unsigned int)fread(buf,1u,sizeof(buf),fp);fclose(fp);
    r=hl2m_repair_missing_first_byte(buf,&n,sizeof(buf),HL2M_FORMAT_MDL,&fixed);
    if(r!=HL2M_OK)return 3;
    r=hl2m_decode_mdl(buf,n,&mdl);if(r!=HL2M_OK){printf("mdl decode %d\n",r);return 4;}
    printf("MODEL %s bones=%d animations=%d sequences=%d repaired=%d\n",mdl.name,mdl.bone_count,mdl.anim_count,mdl.sequence_count,fixed);
    for(i=0;i<mdl.anim_count;i++) {
       const hl2m_mdl_animdesc *a=&mdl.anims[i];
       printf("ANIM %d %-26s frames=%d mov=%d flags=0x%X blocks=%d sections=%d channels=%d channel_flags=0x%X\n",i,a->name,a->numframes,a->nummovements,a->flags,a->animblock,a->sectionframes,a->anim_channel_count,a->anim_flags_or);
    }
    return 0;
}
