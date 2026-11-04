/* C89 regression: identifiers 10 and 100 must never collide in multipart
   retargeting. Bad suffixes must not silently match a legitimate bone. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>
#define CK(x) do{if(!(x)){fprintf(stderr,"failed line %d\n",__LINE__);return 1;}}while(0)
static re4_pmd_skeleton master_bones[130],part_bones[5];
static re4_pmd_name master_names[130],part_names[5];
static char master_str[130][24];
static re4_pmd_model master,part;
static re4_character_links links;
int main(void)
{
    int i;re4_err r;
    memset(&master,0,sizeof(master));memset(&part,0,sizeof(part));
    memset(master_bones,0,sizeof(master_bones));
    memset(part_bones,0,sizeof(part_bones));
    for(i=0;i<130;++i){
        sprintf(master_str[i],"born__%02d",i);
        master_names[i].name.ptr=master_str[i];
        master_names[i].name.len=(re4_size)strlen(master_str[i]);
        master_bones[i].parent=i-1;
    }
    part_names[0].name.ptr="SceneRoot";part_names[0].name.len=9U;
    part_names[1].name.ptr="lh00__10";part_names[1].name.len=8U;
    part_names[2].name.ptr="lh00__100";part_names[2].name.len=9U;
    part_names[3].name.ptr="born__105_S14";part_names[3].name.len=13U;
    part_names[4].name.ptr="born__100X";part_names[4].name.len=10U;
    for(i=0;i<5;++i)part_bones[i].parent=-1;
    master.skeleton_count=master.skeleton_name_count=130U;
    master.skeleton_names=master_names;master.skeletons=master_bones;
    part.skeleton_count=part.skeleton_name_count=5U;
    part.skeleton_names=part_names;part.skeletons=part_bones;
    r=re4_character_link(&master,&part,&links);CK(r==RE4_OK);
    CK(links.master_index[0]==-1);
    CK(links.master_index[1]==10);
    CK(links.master_index[2]==100);
    CK(links.master_index[3]==105);
    CK(links.master_index[4]==-1);
    CK(links.direct_links==3U && links.stationary_links==2U);
    puts("large skeleton bone identity (10 vs 100 vs 105) PASS");
    return 0;
}
