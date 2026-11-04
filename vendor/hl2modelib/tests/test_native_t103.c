#include "hl2modelib.h"
#include <stdio.h>
#include <stdlib.h>
static hl2m_u8 bytes[8388608];
static hl2m_mdl mdl;
static hl2m_pose_matrices am,bm;
int main(int argc, char **argv)
{
    FILE *f; unsigned int n; int fixed,r,i,idx=0,changed=0;hl2m_native_anim_report89 rep;
    if(argc<2)return 1;
    f=fopen(argv[1],"rb");if(!f)return 2;
    n=(unsigned int)fread(bytes,1,sizeof(bytes),f);fclose(f);
    r=hl2m_repair_missing_first_byte(bytes,&n,sizeof(bytes),HL2M_FORMAT_MDL,&fixed);if(r)return 3;
    r=hl2m_decode_mdl(bytes,n,&mdl);if(r)return 4;
    printf("mdl %s: anims %d seqs %d bones %d\n",mdl.name,mdl.anim_count,mdl.sequence_count,mdl.bone_count);
    if(mdl.anim_count<2)return 5;
    idx=1;
    r=hl2m_native_sample_anim_matrices89(bytes,n,&mdl,idx,0,&am,&rep);
    printf("anim %s frame 0 r %d channels %d rot %d pos %d section %d flags %d\n",mdl.anims[idx].name,r,rep.channels,rep.rotation_channels,rep.position_channels,mdl.anims[idx].sectionframes,mdl.anims[idx].anim_flags_or);
    if(r)return 6;
    r=hl2m_native_sample_anim_matrices89(bytes,n,&mdl,idx,mdl.anims[idx].numframes/2,&bm,&rep);
    printf("frame mid r %d channels %d\n",r,rep.channels);if(r)return 7;
    for(i=0;i<am.bone_count;i++)if(am.world[i].m[0][3]!=bm.world[i].m[0][3] || am.world[i].m[1][3]!=bm.world[i].m[1][3] || am.world[i].m[2][3]!=bm.world[i].m[2][3])changed++;
    printf("changed bones %d of %d\n",changed,am.bone_count);
    return changed?0:8;
}
