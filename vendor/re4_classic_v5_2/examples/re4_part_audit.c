/* v3.4 structural attachment audit for arbitrary PMD pairs.
   The result is a candidate link, not a game-verified attachment. C89. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>
#define CAP (16UL*1024UL*1024UL)
#define ARENA_CAP (16UL*1024UL*1024UL)
static re4_u8 master_file[CAP],part_file[CAP],arena_memory[ARENA_CAP];
static re4_skel_matrix master_world[RE4_SKEL_MAX_BONES];
static re4_skel_matrix part_world[RE4_SKEL_MAX_BONES];
static re4_skel_matrix target_world[RE4_SKEL_MAX_BONES];
static re4_character_links links;
static re4_character_link_audit quality;
static re4_pmd_model master,part;
static int load(const char *path,re4_u8 *buf,re4_span *span)
{
    FILE *f=fopen(path,"rb");long size;
    if(!f)return 0;
    if(fseek(f,0,SEEK_END)!=0 || (size=ftell(f))<58L ||
       (unsigned long)size>CAP || fseek(f,0,SEEK_SET)!=0){fclose(f);return 0;}
    if(fread(buf,1,(size_t)size,f)!=(size_t)size){fclose(f);return 0;}
    fclose(f);span->data=buf;span->size=(re4_size)size;return 1;
}
int main(int argc,char **argv)
{
    re4_span mb,pb,empty;re4_arena arena;re4_skel_pose mr,pr,target;
    re4_err err;
    if(argc!=3 && (argc!=4 || strcmp(argv[3],"--dump-bones")!=0)){fputs("usage: re4_part_audit MASTER.pmd PART.pmd [--dump-bones]\n",stderr);return 2;}
    if(!load(argv[1],master_file,&mb)||!load(argv[2],part_file,&pb)){
        puts("IO_OR_LIMIT,0,0,0,0,0,0,0");return 3;
    }
    re4_arena_init(&arena,arena_memory,ARENA_CAP);
    err=re4_pmd_read_survey(mb,&arena,&master);
    if(err!=RE4_OK){puts("MASTER_PARSE_ERROR,0,0,0,0,0,0,0");return 4;}
    err=re4_pmd_read_survey(pb,&arena,&part);
    if(err!=RE4_OK){puts("PART_PARSE_ERROR,0,0,0,0,0,0,0");return 5;}
    err=re4_character_link(&master,&part,&links);
    if(err!=RE4_OK){puts("LINK_ERROR,0,0,0,0,0,0,0");return 6;}
    memset(&mr,0,sizeof(mr));memset(&pr,0,sizeof(pr));memset(&target,0,sizeof(target));
    mr.world=master_world;pr.world=part_world;target.world=target_world;
    empty.data=0;empty.size=0;
    err=re4_skel_evaluate(&master,empty,0,0,&mr,RE4_SKEL_MAX_BONES);
    if(err==RE4_OK)err=re4_skel_evaluate(&part,empty,0,0,&pr,RE4_SKEL_MAX_BONES);
    if(err==RE4_OK)err=re4_character_retarget(&master,&mr,&mr,&part,&pr,
                                               &links,&target,RE4_SKEL_MAX_BONES);
    if(err!=RE4_OK){puts("RETARGET_ERROR,0,0,0,0,0,0,0");return 7;}
    err=re4_character_audit(&master,&part,&links,&quality);
    if(err!=RE4_OK){puts("AUDIT_ERROR,0,0,0,0,0,0,0");return 8;}
    printf("%s,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
           quality.out_of_range_targets?"INVALID_BONE_INDEX":
           quality.ancestry_conflicts?"REVIEW_HIERARCHY":
           quality.repeated_direct_targets?"REVIEW_DUPLICATE":
           quality.direct?"STRUCTURAL_CANDIDATE":"NO_DIRECT_LINK",
           (unsigned long)quality.direct,(unsigned long)quality.inherited,
           (unsigned long)quality.stationary,
           (unsigned long)quality.unique_direct_targets,
           (unsigned long)quality.repeated_direct_targets,
           (unsigned long)quality.ancestry_conflicts,
           (unsigned long)quality.out_of_range_targets);
    if(argc==4){
        re4_u32 i;
        for(i=0;i<part.skeleton_count;++i){
            re4_strref name;name.ptr=0;name.len=0; if(i<part.skeleton_name_count)name=part.skeleton_names[i].name;
            fprintf(stderr,"PART_BONE,%lu,%ld,%.*s,%ld,%u,%ld,%ld,%ld,%ld,%ld,%ld\n",
                (unsigned long)i,(long)part.skeletons[i].parent,
                (int)(name.ptr?name.len:0U),name.ptr?name.ptr:"",
                (long)links.master_index[i],(unsigned)links.link_kind[i],
                (long)pr.world[i].m[3],(long)pr.world[i].m[7],(long)pr.world[i].m[11],
                (long)(links.master_index[i]>=0?mr.world[links.master_index[i]].m[3]:0),
                (long)(links.master_index[i]>=0?mr.world[links.master_index[i]].m[7]:0),
                (long)(links.master_index[i]>=0?mr.world[links.master_index[i]].m[11]:0));
        }
    }
    return 0;
}
