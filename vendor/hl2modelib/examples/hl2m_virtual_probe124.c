/* Recursive Source virtualmodel tester. No runtime allocation. Strict C89.
   usage: probe base.mdl "models/include.mdl=/disk/path/bank.mdl" ...
   ANI files are loaded automatically if present beside each MDL. */
#include "hl2m_master89.h"
#include <stdio.h>
#include <string.h>
#define PROBE124_MAX_MDL 8388608u
#define PROBE124_MAX_ANI 4194304u
static hl2m_u8 mdl_bytes[HL2M_VIRTUAL_GROUP_CAP123][PROBE124_MAX_MDL];
static hl2m_u8 ani_bytes[HL2M_VIRTUAL_GROUP_CAP123][PROBE124_MAX_ANI];
static hl2m_mdl models[HL2M_VIRTUAL_GROUP_CAP123];
static hl2m_virtual_input123 inputs[HL2M_VIRTUAL_GROUP_CAP123];
static char declared[HL2M_VIRTUAL_GROUP_CAP123][HL2M_PATH_MAX];
static hl2m_virtual_multi123 catalog;
static hl2m_virtual_graph124 graph;
static hl2m_pose_matrices pose;
static hl2m_fixed ctrl[HL2M_VIRTUAL_POSE_CAP123];
static hl2m_sequence_layers_report120 report;
static int file_load(const char *path,hl2m_u8 *bytes,
    unsigned int capacity,unsigned int *out)
{
    FILE *f;long len;size_t got;
    f=fopen(path,"rb");if(!f)return 0;
    if(fseek(f,0,SEEK_END)!=0){fclose(f);return 0;}
    len=ftell(f);
    if(len<0 || (unsigned long)len>capacity || fseek(f,0,SEEK_SET)!=0)
       {fclose(f);return 0;}
    got=fread(bytes,1u,(size_t)len,f);fclose(f);
    if(got!=(size_t)len)return 0;
    *out=(unsigned int)len;return 1;
}
int main(int argc,char **argv)
{
    int i,j,r,v,selected=-1,owner;
    const char *local;
    char anipath[1024];
    unsigned int size,anisize;
    if(argc<3||argc>HL2M_VIRTUAL_GROUP_CAP123+1){
        fprintf(stderr,"usage: %s base.mdl include_path=local_bank.mdl [...]\n",argv[0]);return 2;
    }
    for(i=0;i<argc-1;i++){
        local=argv[i+1];
        if(i>0){
            const char *sep=strchr(local,'=');
            size_t n;
            if(!sep){fprintf(stderr,"missing include_path= prefix\n");return 2;}
            n=(size_t)(sep-local);
            if(!n||n>=HL2M_PATH_MAX)return 2;
            memcpy(declared[i],local,n);declared[i][n]=0;
            inputs[i].include_path=declared[i];
            local=sep+1;
        }
        if(!file_load(local,mdl_bytes[i],PROBE124_MAX_MDL,&size)){
            fprintf(stderr,"could not load %s\n",local);return 2;
        }
        inputs[i].mdl_bytes=mdl_bytes[i];inputs[i].mdl_size=size;
        inputs[i].mdl=&models[i];
        r=hl2m_decode_mdl(mdl_bytes[i],size,&models[i]);
        if(r){fprintf(stderr,"MDL %s decode %d\n",local,r);return 2;}
        anisize=0u;
        if(strlen(local)<sizeof(anipath)-1u){
            strcpy(anipath,local);j=(int)strlen(anipath);
            if(j>4 && strcmp(anipath+j-4,".mdl")==0){
                strcpy(anipath+j-4,".ani");
                if(file_load(anipath,ani_bytes[i],PROBE124_MAX_ANI,&anisize)){
                    inputs[i].ani_bytes=ani_bytes[i];inputs[i].ani_size=anisize;
                }
            }
        }
    }
    r=hl2m_virtual_build_recursive124(inputs,(unsigned int)(argc-1),
        &catalog,&graph);
    if(r){fprintf(stderr,"recursive virtual build: %d\n",r);return 3;}
    printf("RECURSIVE124 groups=%d sequences=%d animations=%d poses=%d\n",
        catalog.group_count,catalog.seq_count,catalog.anim_count,catalog.pose_count);
    for(i=0;i<catalog.group_count;i++){
        int origin=graph.group_to_input[i];
        printf(" GROUP %d input=%d parent=%d seq=%d mapped=%d parents_conflict=%d\n",
            i,origin,graph.parent_group[i],catalog.groups[i].source.mdl->sequence_count,
            catalog.groups[i].mapped_bones,catalog.groups[i].parent_conflicts);
    }
    for(i=0;i<catalog.pose_count;i++)ctrl[i]=HL2M_FIXED_ONE/2;
    for(v=0;v<catalog.seq_count;v++){
        hl2m_virtual_ref122 ref=catalog.sequences[v];
        hl2m_seq_metadata89 meta;
        const hl2m_virtual_group123 *g=&catalog.groups[ref.group];
        r=hl2m_sequence_metadata89(g->source.mdl_bytes,g->source.mdl_size,
            g->source.mdl,ref.local,&meta);
        if(!r&&ref.group>0&&meta.layer_count>0){
            int okay=1,z;
            for(z=0;z<meta.layer_count;z++){
                int child=meta.layers[z].sequence;
                if(child<0||child>=g->source.mdl->sequence_count ||
                    (g->source.mdl->sequences[child].flags&HL2M_SEQ_DELTA89))
                    okay=0;
            }
            if(okay){selected=v;break;}
        }
    }
    if(selected<0){
        for(v=0;v<catalog.seq_count;v++)if(catalog.sequences[v].group>0){selected=v;break;}
    }
    if(selected<0)return 0;
    owner=catalog.sequences[selected].group;
    printf(" selected_virtual=%d owner_group=%d label=%s\n",selected,owner,
        catalog.groups[owner].source.mdl->sequences[catalog.sequences[selected].local].label);
    r=hl2m_master_virtual_sequence_layers124(&catalog,selected,
        HL2M_FIXED_ONE/4,ctrl,(unsigned int)catalog.pose_count,&pose,&report);
    printf(" playback_status=%d bones=%d layers_declared=%d applied=%d\n",
        r,r?0:pose.bone_count,report.declared,report.applied);
    return r?4:0;
}
