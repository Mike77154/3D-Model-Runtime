/* Structural attachment link smoke-test for arbitrary PMD pairs.
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
    if(argc!=3){fputs("usage: re4_part_probe MASTER.pmd PART.pmd\n",stderr);return 2;}
    if(!load(argv[1],master_file,&mb)||!load(argv[2],part_file,&pb)){
        puts("IO_OR_LIMIT,0,0,0");return 3;
    }
    re4_arena_init(&arena,arena_memory,ARENA_CAP);
    err=re4_pmd_read_survey(mb,&arena,&master);
    if(err!=RE4_OK){puts("MASTER_PARSE_ERROR,0,0,0");return 4;}
    err=re4_pmd_read_survey(pb,&arena,&part);
    if(err!=RE4_OK){puts("PART_PARSE_ERROR,0,0,0");return 5;}
    err=re4_character_link(&master,&part,&links);
    if(err!=RE4_OK){puts("LINK_ERROR,0,0,0");return 6;}
    memset(&mr,0,sizeof(mr));memset(&pr,0,sizeof(pr));memset(&target,0,sizeof(target));
    mr.world=master_world;pr.world=part_world;target.world=target_world;
    empty.data=0;empty.size=0;
    err=re4_skel_evaluate(&master,empty,0,0,&mr,RE4_SKEL_MAX_BONES);
    if(err==RE4_OK)err=re4_skel_evaluate(&part,empty,0,0,&pr,RE4_SKEL_MAX_BONES);
    if(err==RE4_OK)err=re4_character_retarget(&master,&mr,&mr,&part,&pr,
                                               &links,&target,RE4_SKEL_MAX_BONES);
    if(err!=RE4_OK){puts("RETARGET_ERROR,0,0,0");return 7;}
    printf("%s,%lu,%lu,%lu\n",links.direct_links?"LINK_CANDIDATE":"NO_DIRECT_LINK",
           (unsigned long)links.direct_links,(unsigned long)links.inherited_links,
           (unsigned long)links.stationary_links);
    return 0;
}
