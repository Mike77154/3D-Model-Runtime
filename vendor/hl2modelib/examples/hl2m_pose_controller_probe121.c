/* Real Resident Evil Source49 MDL normalized controller probe, no heap. */
#include "hl2m_master89.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MDL_CAP (12u*1024u*1024u)
static hl2m_u8 mdl_data[MDL_CAP];
static hl2m_mdl mdl;
static hl2m_pose_matrices actual,legacy,quarter;
static hl2m_poseparam121 desc[HL2M_POSEPARAM_CAP121];
static hl2m_fixed ctl[HL2M_POSEPARAM_CAP121];
static unsigned int load(const char *path)
{
    FILE *f=fopen(path,"rb");size_t n;
    if(!f)return 0u;
    n=fread(mdl_data,1u,MDL_CAP,f);
    if(!feof(f)){fclose(f);return 0u;}
    fclose(f);return (unsigned int)n;
}
int main(int argc,char **argv)
{
    unsigned int size,np=0u,i;int r,idx,used[4],differences=0,apply_layers=0;
    hl2m_fixed px,py,cycle=32768;int changed=0;
    hl2m_sequence_layers_report120 report;
    if(argc!=3 && argc!=4){puts("Usage: hl2m_pose_controller_probe121 model.mdl sequence_index [apply_layers]");return 1;}
    if(argc==4)apply_layers=atoi(argv[3]);
    size=load(argv[1]);if(!size){puts("Cannot load MDL");return 2;}
    r=hl2m_master_decode89(HL2M_FORMAT_MDL,mdl_data,size,NULL,0,&mdl);
    if(r){printf("MDL decode error %d\n",r);return 3;}
    idx=atoi(argv[2]);if(idx<0 || idx>=mdl.sequence_count)return 4;
    r=hl2m_poseparams_read121(mdl_data,size,desc,HL2M_POSEPARAM_CAP121,&np);
    if(r){printf("Pose descriptor error %d\n",r);return 5;}
    for(i=0u;i<np;i++)ctl[i]=49152;
    r=hl2m_pose_sequence_axes121(&mdl,idx,desc,np,ctl,np,&px,&py);
    if(r)return 6;
    r=hl2m_master_sequence_controls121(mdl_data,size,NULL,0u,&mdl,idx,cycle,
        ctl,np,apply_layers,&actual,&report);
    if(r){printf("Source controller sample failed: %d\n",r);return 7;}
    if(apply_layers){
        r=hl2m_master_sequence_layers120(mdl_data,size,NULL,0u,&mdl,idx,cycle,
            px,py,NULL,0u,&legacy,&report);
    }else{
        r=hl2m_master_sequence_pose89(mdl_data,size,NULL,0u,&mdl,idx,cycle,
            px,py,&legacy,used);
    }
    if(r){printf("Physical-axis baseline failed: %d\n",r);return 8;}
    for(i=0u;i<(unsigned int)actual.bone_count;i++){
        if(memcmp(&actual.local[i],&legacy.local[i],sizeof(actual.local[i]))!=0)
            differences++;
    }
    printf("MODEL %s SEQUENCE %s PARAMS %u AXIS0 %ld AXIS1 %ld BONES %d DIFF_BONES %d\n",
        mdl.name,mdl.sequences[idx].label,np,(long)px,(long)py,
        actual.bone_count,differences);
    for(i=0u;i<np;i++)ctl[i]=16384;
    r=hl2m_master_sequence_controls121(mdl_data,size,NULL,0u,&mdl,idx,cycle,
        ctl,np,apply_layers,&quarter,&report);
    if(r){printf("Control 0.25 sample failed: %d\n",r);return 11;}
    for(i=0u;i<(unsigned int)actual.bone_count;i++){
        if(memcmp(&actual.local[i],&quarter.local[i],sizeof(actual.local[i]))!=0)
            changed++;
    }
    printf("CONTROL_Q25_TO_Q75_CHANGED_BONES %d\n",changed);
    for(i=0u;i<np;i++){
        hl2m_fixed physical;
        r=hl2m_pose_physical_from_control121(&desc[i],49152,&physical);
        if(r)return 9;
        printf("CONTROL %u %s START %ld END %ld LOOP %ld Q75_PHYSICAL %ld\n",
            i,desc[i].name,(long)desc[i].start,(long)desc[i].end,
            (long)desc[i].loop,(long)physical);
    }
    return differences?10:0;
}
